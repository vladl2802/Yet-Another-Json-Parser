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

namespace detail {
    using Token = Lexer::Token;
    enum TokenPattern {
        begin = Token::begin_array | Token::begin_object,
        end = Token::end_array | Token::end_object,

        boolean = Token::boolean,

        null = Token::null,

        unsigned_integer = Token::signed_number,
        signed_integer = Token::unsigned_number | unsigned_integer,
        number = Token::float_number | signed_integer,

        string = Token::string,

        value = begin | boolean | null | number | string
    };
    enum class StructureParsingFlag {for_each};

    enum class StructureToken {array, object, callback, unknown};
    using ParserVariablePath = std::variant<uint32_t, std::string, const char*, StructureParsingFlag>;

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
}

class ParserUnit;

class JsonParser {
    friend ParserUnit;

    explicit JsonParser(IStreamWrapper&&);

    static bool check_pattern(detail::Token tk, detail::TokenPattern pat);
    ParserUnit* const base_parser_unit();

    inline detail::Token scan_token(bool memorise = true);
    inline detail::Token expect_token(detail::Token, bool memorise = false);
    inline detail::Token expect_token(uint16_t, bool memorise = false);
    inline detail::Token expect_token(detail::TokenPattern, bool memorise = false);

    Lexer lexer;
    std::vector<ParserUnit*> units;
};

struct BasicParserCallback  {
    virtual void invoke(ParserUnit*, const ParserUnit*) = 0;

    BasicParserCallback() = default;
    virtual ~BasicParserCallback() = default;
};

class ParserUnit {
    using Token = detail::Token;
    using StructureToken = detail::StructureToken;
    using TokenPattern = detail::TokenPattern;
    using ParserVariablePath = detail::ParserVariablePath;

    friend ParserUnit;

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
    void add_variable_listener(T&, std::initializer_list<ParserVariablePath> path);
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

    explicit ParserUnit(JsonParser&);

private:

    using ArrayVertex = detail::NestingVertex<uint32_t>;
    using ObjectVertex = detail::NestingVertex<std::string>;

    struct CallbackVertex : public detail::Vertex {
        std::unique_ptr<BasicParserCallback> callback;

        CallbackVertex(StructureToken, Vertex*, std::unique_ptr<BasicParserCallback>);
    };

    [[nodiscard]] static detail::Vertex* create_vertex(StructureToken, std::unique_ptr<BasicParserCallback>);

    JsonParser &parent;
    std::vector<detail::Vertex*> vertex_stack;
};

// the rest of functions are located in .cpp

template<typename type, detail::TokenPattern pattern_token, detail::Token type_token>
ParserUnit::VariableParserCallback<type, pattern_token, type_token>::VariableParserCallback(
        type &variable) : variable(variable) {}

template<typename type, detail::TokenPattern pattern_token, detail::Token type_token>
void ParserUnit::VariableParserCallback<type, pattern_token, type_token>::invoke(
        ParserUnit* const child_pu, const ParserUnit* const parent_pu) {
    auto &par = child_pu->parent;
    par.expect_token(pattern_token, true);
    this->variable = par.lexer.get<type_token>();
}

template<typename T> requires std::is_same_v<T, bool>
void ParserUnit::add_variable_listener(T &variable, std::initializer_list<ParserVariablePath> path) {
    add_callback_listener(std::make_unique<VariableParserCallback<T, TokenPattern::boolean, Token::boolean>>(variable), path);
}

template<typename T> requires std::is_convertible_v<T, std::string>
void ParserUnit::add_variable_listener(T &variable, std::initializer_list<ParserVariablePath> path) {
    add_callback_listener(std::make_unique<VariableParserCallback<T, TokenPattern::string, Token::string>>(variable), path);
}

template<typename T> requires std::is_integral_v<T> && std::is_unsigned_v<T> && (!std::is_same_v<T, bool>)
void ParserUnit::add_variable_listener(T &variable, std::initializer_list<ParserVariablePath> path) {
    add_callback_listener(std::make_unique<VariableParserCallback<T, TokenPattern::unsigned_integer, Token::unsigned_number>>(variable), path);
}

template<typename T> requires std::is_integral_v<T> && std::is_signed_v<T> && (!std::is_same_v<T, bool>)
void ParserUnit::add_variable_listener(T &variable, std::initializer_list<ParserVariablePath> path) {
    add_callback_listener(std::make_unique<VariableParserCallback<T, TokenPattern::signed_integer, Token::signed_number>>(variable), path);
}

template<typename T> requires std::is_floating_point_v<T>
void ParserUnit::add_variable_listener(T &variable, std::initializer_list<ParserVariablePath> path) {
    add_callback_listener(std::make_unique<VariableParserCallback<T, TokenPattern::number, Token::float_number>>(variable), path);
}

template<typename T>
detail::NestingVertex<T>::NestingVertex(StructureToken type, detail::Vertex* go_for_each, std::map<T, Vertex*> to_go, T last)
        : Vertex(type, go_for_each), to_go(to_go), last(last) {}

#endif //COMBPROJECT_JSON_PARSER_H
