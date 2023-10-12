//
// Created by Vlad on 12.07.2023.
//

#ifndef JSONPARSER_LEXER_H
#define JSONPARSER_LEXER_H

#include <cstdint>
#include <string>
#include "stream_wrapper.h"

class Lexer {
public:
    enum Token {
        boolean = 1,
        null = 2,
        unsigned_number = 4,
        signed_number = 8,
        float_number = 16,
        string = 32,
        begin_array = 64,
        end_array = 128,
        begin_object = 256,
        end_object = 512,
        name_delimiter = 1024,
        element_delimiter = 2048,
        eof = 4096,
        error = 8192
    };
private:

    using char_int_t = std::char_traits<char>::int_type;

    IStreamWrapper stream;

    std::string buffer;
    char_int_t current = std::char_traits<char>::eof();
    bool to_unget = false;

    uint32_t line_number = 0;
    uint16_t pos_in_line = 0;

    char_int_t get();
    void unget(); /// @note pos_in_line invalidates here, but it will become valid on the next line (after \n).
                  /// Since we can unget only one time this is not critical.
    [[nodiscard]] std::string current_character_position() const;

    bool skip_BOM();
    void skip_whitespace();

    Token scan_literals(bool memorise = true);

    Token scan_numbers(bool memorise = true);
    Token scan_real_part(bool memorise = true);
    Token scan_fraction_part(bool memorise = true);
    Token scan_exponent_part(bool memorise = true);

    Token scan_string(bool memorise = true);
    Token scan_escaped_character(bool memorise = true); // convert UTF-16 to UTF-8
    int scan_codepoint();
    static std::string convert_pure_to_UTF8(int pure);
    Token scan_unicode_character(bool memorise = true);

public:
    std::string error_message; // last caught error message

    explicit Lexer(IStreamWrapper&& is);
    [[nodiscard]] std::string get_token_name(Token tk) const;
    Token scan_token(bool memorise = true);

    // this all make sense after scan_token(memorise = true);
    [[nodiscard]] bool get_bool() const;
    [[nodiscard]] uint64_t get_unsigned() const;
    [[nodiscard]] int64_t get_signed() const;
    [[nodiscard]] long double get_float() const;
    [[nodiscard]] std::string get_string() const;
    template<Lexer::Token tk>[[nodiscard]] auto get() const;
};

// the rest of functions are located in .cpp

template<Lexer::Token tk> auto Lexer::get() const {
    if constexpr (tk == Token::boolean) return get_bool();
    else if constexpr (tk == Token::unsigned_number) return get_unsigned();
    else if constexpr (tk == Token::signed_number) return get_signed();
    else if constexpr (tk == Token::float_number) return get_float();
    else if constexpr (tk == Token::string) return get_string();
    else []<bool flag = false>() {static_assert(flag, "Wrong token to get");}();
}

#endif //JSONPARSER_LEXER_H
