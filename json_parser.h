//
// Created by Vlad on 10.07.2023.
//

#ifndef COMBPROJECT_JSON_PARSER_H
#define COMBPROJECT_JSON_PARSER_H

#include <iostream>
#include <utility>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <cassert>
#include <memory>
#include <variant>

#include "lexer.h"
#include "stream_wrapper.h"

class JsonParser {
    using Token = Lexer::Token;
    enum class TokenPattern {
        begin = std::to_underlying(Token::begin_array) | std::to_underlying(Token::begin_object),
        end = std::to_underlying(Token::end_array) | std::to_underlying(Token::end_object),

        boolean = std::to_underlying(Token::boolean),

        null = std::to_underlying(Token::null),

        unsigned_integer = std::to_underlying(Token::signed_number),
        signed_integer = std::to_underlying(Token::unsigned_number) | unsigned_integer,
        number = std::to_underlying(Token::float_number) | signed_integer,

        string = std::to_underlying(Token::string),

        value = begin | boolean | null | number | string
    };
    enum class StructureParsingFlag {for_each};
    using ParserVariablePath = std::variant<uint32_t, std::string, const char*, StructureParsingFlag>;

    enum class StructureToken {array, object, callback, unknown};
public:

    class ParserUnit;

    explicit JsonParser(IStreamWrapper&&);

    ParserUnit* const base_parser_unit();

    struct BasicParserCallback  {
        virtual void invoke(ParserUnit*, const ParserUnit*) = 0;

        BasicParserCallback() = default;
        virtual ~BasicParserCallback() = default;
    };

    class ParserUnit {

        friend JsonParser;

        template<typename type, TokenPattern pattern_token, Token type_token>
        struct VariableParserCallback final : BasicParserCallback {
            type& variable;

            explicit VariableParserCallback(type &variable);
            ~VariableParserCallback() override = default;

            void invoke(ParserUnit*, const ParserUnit*) override;
        };

    public:

        ParserUnit() = delete;

        template<typename T> requires std::is_same_v<T, bool>
        void add_variable_listener(T&, std::initializer_list<ParserVariablePath>);
        template<typename T> requires std::is_convertible_v<T, std::string>
        void add_variable_listener(T&, std::initializer_list<ParserVariablePath> path);
        template<typename T> requires std::is_integral_v<T> && std::is_unsigned_v<T> && (!std::is_same_v<T, bool>)
        void add_variable_listener(T&, std::initializer_list<ParserVariablePath> path);
        template<typename T> requires std::is_integral_v<T> && std::is_signed_v<T> && (!std::is_same_v<T, bool>)
        void add_variable_listener(T&, std::initializer_list<ParserVariablePath> path);
        template<typename T> requires std::is_floating_point_v<T>
        void add_variable_listener(T&, std::initializer_list<ParserVariablePath> path);

        void add_callback_listener(std::unique_ptr<BasicParserCallback>, std::initializer_list<ParserVariablePath> path);

        void scan();

    private:

        struct Vertex {
            StructureToken type;
            Vertex* go_for_each;

            virtual ~Vertex() = default;
            Vertex(StructureToken, Vertex*);
        };

        template<typename T>
        struct NestingVertex : public Vertex {
            std::map<T, Vertex*> to_go;
            T last;

            NestingVertex(StructureToken, Vertex*, std::map<T, Vertex*>, T);
        };

        using ArrayVertex = NestingVertex<uint32_t>;
        using ObjectVertex = NestingVertex<std::string>;

        struct CallbackVertex : public Vertex {
            std::unique_ptr<BasicParserCallback> callback;

            CallbackVertex(StructureToken, Vertex*, std::unique_ptr<BasicParserCallback>);
        };

        explicit ParserUnit(JsonParser&);

        [[nodiscard]] static Vertex* create_vertex(StructureToken, std::unique_ptr<BasicParserCallback>);

        JsonParser &parent;
        std::vector<Vertex*> vertex_stack;
    };

private:

    static bool check_pattern(Token tk, TokenPattern pat);

    inline Token scan_token(bool memorise = true);
    inline Token expect_token(Token, bool memorise = false);
    inline Token expect_token(uint16_t, bool memorise = false);
    inline Token expect_token(TokenPattern, bool memorise = false);

    Lexer lexer;
    std::vector<ParserUnit*> units;
};

// the rest of functions are located in .cpp

template<typename type, JsonParser::TokenPattern pattern_token, JsonParser::Token type_token>
JsonParser::ParserUnit::VariableParserCallback<type, pattern_token, type_token>::VariableParserCallback(
        type &variable) : variable(variable) {}

template<typename type, JsonParser::TokenPattern pattern_token, JsonParser::Token type_token>
void JsonParser::ParserUnit::VariableParserCallback<type, pattern_token, type_token>::invoke(
        JsonParser::ParserUnit* const child_pu, const JsonParser::ParserUnit* const parent_pu) {
    auto &par = child_pu->parent;
    par.expect_token(pattern_token, true);
    this->variable = par.lexer.get<type_token>();
}

template<typename T> requires std::is_same_v<T, bool>
void JsonParser::ParserUnit::add_variable_listener(T &variable, std::initializer_list<ParserVariablePath> path) {
    add_callback_listener(std::make_unique<VariableParserCallback<T, TokenPattern::boolean, Token::boolean>>(variable), path);
}

template<typename T> requires std::is_convertible_v<T, std::string>
void JsonParser::ParserUnit::add_variable_listener(T &variable, std::initializer_list<ParserVariablePath> path) {
    add_callback_listener(std::make_unique<VariableParserCallback<T, TokenPattern::string, Token::string>>(variable), path);
}

template<typename T> requires std::is_integral_v<T> && std::is_unsigned_v<T> && (!std::is_same_v<T, bool>)
void JsonParser::ParserUnit::add_variable_listener(T &variable, std::initializer_list<ParserVariablePath> path) {
    add_callback_listener(std::make_unique<VariableParserCallback<T, TokenPattern::unsigned_integer, Token::unsigned_number>>(variable), path);
}

template<typename T> requires std::is_integral_v<T> && std::is_signed_v<T> && (!std::is_same_v<T, bool>)
void JsonParser::ParserUnit::add_variable_listener(T &variable, std::initializer_list<ParserVariablePath> path) {
    add_callback_listener(std::make_unique<VariableParserCallback<T, TokenPattern::signed_integer, Token::signed_number>>(variable), path);
}

template<typename T> requires std::is_floating_point_v<T>
void JsonParser::ParserUnit::add_variable_listener(T &variable, std::initializer_list<ParserVariablePath> path) {
    add_callback_listener(std::make_unique<VariableParserCallback<T, TokenPattern::number, Token::float_number>>(variable), path);
}

template<typename T>
JsonParser::ParserUnit::NestingVertex<T>::NestingVertex(JsonParser::StructureToken type, JsonParser::ParserUnit::Vertex* go_for_each, std::map<T, Vertex*> to_go, T last)
        : Vertex(type, go_for_each), to_go(to_go), last(last) {}

#endif //COMBPROJECT_JSON_PARSER_H
