//
// Created by Vlad on 12.07.2023.
//

#include <cassert>
#include <sstream>

#include "lexer.h"

Lexer::Lexer(IStreamWrapper&& is) : stream(std::move(is)) {}

Lexer::char_int_t Lexer::get() {
    if (to_unget) {
        to_unget = false;
    } else {
        if (current == '\n') {
            pos_in_line = 0;
            line_number++;
        } else pos_in_line++;
        current = stream.get();
    }
    return current;
}

void Lexer::unget() {
    if (to_unget) assert(false);
    to_unget = true;
    if (pos_in_line == 0) {
        if (line_number > 0) line_number--;
    } else pos_in_line--;
}

std::string Lexer::current_character_position() const {
    std::stringstream res;
    res << "'" << static_cast<char>(current) << "'(" << std::hex << static_cast<int>(current) << std::dec <<
    ") at " << line_number << ":" << pos_in_line;
    return res.str();
}

bool Lexer::skip_BOM() {
    get();
    if (current == 0xEF) {
        return get() == 0xBB && get() == 0xBF;
    } else unget();
    return true;
}

void Lexer::skip_whitespace() {
    while (current == ' ' || current == '\n' || current == '\r' || current == '\t') {
        get();
    }
}

Lexer::Token Lexer::scan_token(bool memorise) {
    if (memorise) buffer.clear();
    if (line_number == 0 && pos_in_line == 0) { [[unlikely]]
        if (!skip_BOM()) {
            error_message = "Found character " + current_character_position() +
                            " which not corresponds to BOM (0xEF 0xBB 0xBF) but it should.";
            return Token::error;
        }
        get();
    }
    skip_whitespace();
    Token res;
    switch (current) {
        case '\0': [[fallthrough]];  // for future possibility to scan json from string terminated with \0
        case std::char_traits<char>::eof():
            res = Token::eof;
            break;
        case '{':
            res = Token::begin_object;
            break;
        case '}':
            res = Token::end_object;
            break;
        case '[':
            res = Token::begin_array;
            break;
        case ']':
            res = Token::end_array;
            break;
        case ':':
            res = Token::name_delimiter;
            break;
        case ',':
            res = Token::element_delimiter;
            break;
        case '"':
            res = scan_string(memorise);
            break;
        case '-': [[fallthrough]];
        case '0': [[fallthrough]];
        case '1': [[fallthrough]];
        case '2': [[fallthrough]];
        case '3': [[fallthrough]];
        case '4': [[fallthrough]];
        case '5': [[fallthrough]];
        case '6': [[fallthrough]];
        case '7': [[fallthrough]];
        case '8': [[fallthrough]];
        case '9':
            res = scan_numbers(memorise);
            unget();
            break;
        case 't': [[fallthrough]];
        case 'f': [[fallthrough]];
        case 'n':
            res = scan_literals(memorise);
            unget();
            break;
        default:
            error_message = "character " + current_character_position() + " does not corresponds to JSON structure.";
            res = Token::error;
    }
    get();
    return res;
}

Lexer::Token Lexer::scan_literals(bool memorise) {
    std::string must_be;
    switch (current) {
        case 't':  // true
            must_be = "true";
            if (get() == 'r' && get() == 'u' && get() == 'e') {
                get();
                if (memorise) buffer = "1";
                return Token::boolean;
            }
            break;
        case 'f':  // false
            must_be = "false";
            if (get() == 'a' && get() == 'l' && get() == 's' && get() == 'e') {
                get();
                if (memorise) buffer = "0";
                return Token::boolean;
            }
            break;
        case 'n': // null
            must_be = "null";
            if (get() == 'u' && get() == 'l' && get() == 'l') {
                get();
                return Token::null;
            }
            break;

    }
    error_message = "Invalid literal: It must be " + must_be + " but character " + current_character_position() + "is wrong.";
    return Token::error;
}

Lexer::Token Lexer::scan_numbers(bool memorise) {
    Token res;
    bool has_sign = false;
    if (current == '-') {
        get();
        has_sign = true;
        if (memorise) buffer.push_back('-');
    }
    res = scan_real_part(memorise);
    if (current == '.') {
        if (memorise) buffer.push_back('.');
        res = scan_fraction_part(memorise);
    }
    if (current == 'E' || current == 'e') {
        if (memorise) buffer.push_back('e');
        res = scan_exponent_part(memorise);
    }
    if (res == Token::unsigned_number) {
        return has_sign ? Token::signed_number : Token::unsigned_number;
    }
    assert(res == Token::float_number || res == Token::error);
    return res;
}

Lexer::Token Lexer::scan_real_part(bool memorise) {
    if (current == '0') {
        if (memorise) buffer.push_back('0');
        get();
        return Token::unsigned_number;
    } else if ('1' <= current && current <= '9') {
        if (memorise) buffer.push_back(static_cast<char>(current));
    } else {
        error_message = "Invalid number: Expected digit 1, but found character " + current_character_position() + ".";
        return Token::error;
    }
    while(true) {
        get();
        if ('0' <= current && current <= '9') {
            if (memorise) buffer.push_back(static_cast<char>(current));
        }
        else break;
    }
    return Token::unsigned_number;
}

Lexer::Token Lexer::scan_fraction_part(bool memorise) {
    bool first = true;
    while (true) {
        get();
        if ('0' <= current && current <= '9') {
            if (memorise) buffer.push_back(static_cast<char>(current));
        } else {
            if (first) {
                error_message = "Invalid number: Expected digit 2, but found character " + current_character_position() + ".";
                return Token::error;
            }
            break;
        }
        first = false;
    }
    return Token::float_number;
}

Lexer::Token Lexer::scan_exponent_part(bool memorise) {
    get();
    bool first = true;
    if (current == '-' || current == '+') {
        if (memorise) buffer.push_back(static_cast<char>(current));
        get();
    } else if ('0' <= current && current <= '9') {
        first = false;
    } else {
        error_message = "Invalid number: Expected digit 3, but found character " + current_character_position() + ".";
        return Token::error;
    }
    while (true) {
        if ('0' <= current && current <= '9') {
            if (memorise) buffer.push_back(static_cast<char>(current));
        } else {
            if (first) {
                error_message = "Invalid number: Expected digit 4, but found character " + current_character_position() + ".";
                return Token::error;
            }
            break;
        }
        first = false;
        get();
    }
    return Token::float_number;
}

Lexer::Token Lexer::scan_string(bool memorise) {
    Lexer::Token res;
    bool scan = true;
    while (scan) {
        get();
        switch (current) {
            case '"':
                res = Token::string;
                scan = false;
                break;
            case '/':
                get();
                switch (current) {
                    case '"': [[fallthrough]];
                    case '\\': [[fallthrough]];
                    case '/':
                        if (memorise) buffer.push_back(static_cast<char>(current));
                        break;
                    case 'b':
                        if (memorise) buffer.push_back('\b');
                        break;
                    case 'f':
                        if (memorise) buffer.push_back('\f');
                        break;
                    case 'n':
                        if (memorise) buffer.push_back('\n');
                        break;
                    case 'r':
                        if (memorise) buffer.push_back('\r');
                        break;
                    case 't':
                        if (memorise) buffer.push_back('\t');
                        break;
                    case 'u':
                        res = scan_escaped_character(memorise);
                        break;
                    default:
                        error_message = "Invalid string: After '/' character " + current_character_position() + " not expected.";
                        res = Token::error;
                }
                break;
            default:
                res = scan_unicode_character(memorise);
        }
    }
    return res;
}

Lexer::Token Lexer::scan_escaped_character(bool memorise) {
    int pure_unicode;
    int cd1 = scan_codepoint();
    if (cd1 < 0 || (1 << 16) <= cd1) {
        // error message from scan_codepoint
        return Token::error;
    }
    if (0xD800 <= cd1 && cd1 <= 0xD8FF) {
        if (get() == '/' && get() == 'u') {
            int cd2 = scan_codepoint();
            if (0xDC00 <= cd2 && cd2 <= 0xDFFF) {
                cd1 -= 0xD800;
                cd2 -= 0xDC00;
                pure_unicode = (cd1 << 10) + cd2 + 0x10000;
            } else {
                error_message = "Invalid string: Low surrogate value in UTF-16 should be in 0xDC00 .. 0xDFFF.";
                return Token::error;
            }
        } else {
            error_message = "Invalid string: Expected /u to continue surrogate UTF-16 character, but found character " + current_character_position() + ".";
            return Token::error;
        }
    } else pure_unicode = cd1;
    if (0 <= pure_unicode && pure_unicode <= 0x10FFFF) {
        if (memorise) buffer += convert_pure_to_UTF8(pure_unicode);
    } else {
        error_message = "Invalid string: Unicode value should be in 0 .. 0x10FFFF, but get " + std::to_string(pure_unicode) + ".";
        return Token::error;
    }
    return Token::string;
}

int Lexer::scan_codepoint() {
    int res = 0;
    for (uint8_t i = 3; i >= 0; --i) {
        get();
        uint8_t dig;
        if ('0' <= current && current <= '9') dig = current - '0';
        else if ('a' <= current && current <= 'f') dig = current - 'a' + 10;
        else if ('A' <= current && current <= 'F') dig = current - 'A' + 10;
        else {
            error_message = "Invalid string: Expected hex digit but found " + current_character_position() + ".";
            return -1;
        }
        res += dig * (1 << (i * 4));
    }
    return res;
}

std::string Lexer::convert_pure_to_UTF8(int pure) {
    std::string res;
    if (pure <= 0x7F) { // 1 byte
        res.push_back(static_cast<char>(pure));
    } else if (pure <= 0x7FF) { // 2 bytes
        res.push_back(static_cast<char>(0xC0 | (pure >> 6)));
        res.push_back(static_cast<char>(0x80 | ((pure) & 0x3F)));
    } else if (pure <= 0xFFFF) { // 3 bytes
        res.push_back(static_cast<char>(0xE0 | (pure >> 12)));
        res.push_back(static_cast<char>(0x80 | ((pure >> 6) & 0x3F)));
        res.push_back(static_cast<char>(0x80 | ((pure) & 0x3F)));
    } else /* if (pure <= 0x10FFFF) */ { // 4 bytes
        res.push_back(static_cast<char>(0xF0 | (pure >> 18)));
        res.push_back(static_cast<char>(0x80 | ((pure >> 12) & 0x3F)));
        res.push_back(static_cast<char>(0x80 | ((pure >> 6) & 0x3F)));
        res.push_back(static_cast<char>(0x80 | ((pure) & 0x3F)));
    }
    return res;
}

Lexer::Token Lexer::scan_unicode_character(bool memorise) {
    if (current <= 0x1F) {
        error_message = "Invalid string: String can not contain control character " + current_character_position() + ".";
        return Token::error;
    }
    if (memorise) buffer.push_back(static_cast<char>(current));
    uint8_t bytes = 0;
    for (uint8_t i = 7; i >= 3; --i) {
        if (current & (1 << i)) bytes++;
        else break;
    }
    if (bytes < 4) {
        error_message = "Invalid string: Unicode character should started with 0..3 ones and followed with zero.";
        return Token::error;
    }
    if (bytes < 7) bytes--;
    for (uint8_t i = 0; i < bytes; ++i) {
        get();
        if (0x80 <= current && current <= 0xBF) {
            if (memorise) buffer.push_back(static_cast<char>(current)); // match pattern 10xxxxxx
        }
        else {
            error_message = "Invalid string: Unicode character should continue with 10xxxxxx.";
            return Token::error;
        }
    }
    return Token::string;
}

std::string Lexer::get_token_name(Lexer::Token tk) const {
    switch (tk) {
        case Token::boolean:
            return "boolean";
        case Token::null:
            return "null";
        case Token::unsigned_number:
            return "unsigned_number";
        case Token::signed_number:
            return "signed_number";
        case Token::float_number:
            return "float_number";
        case Token::string:
            return "string";
        case Token::begin_array:
            return "begin_array";
        case Token::end_array:
            return "end_array";
        case Token::begin_object:
            return "begin_object";
        case Token::end_object:
            return "end_object";
        case Token::name_delimiter:
            return "name_delimiter";
        case Token::element_delimiter:
            return "element_delimiter";
        case Token::eof:
            return "eof";
        case Token::error:
            return " <lexer error> " + error_message;
        default:
            return "nothing";
    }
}

bool Lexer::get_bool() const {
    assert(buffer == "False" || buffer == "True");
    return buffer == "True";
}

uint64_t Lexer::get_unsigned() const {
    return std::stoull(buffer);
}

int64_t Lexer::get_signed() const {
    return std::stoll(buffer);
}

long double Lexer::get_float() const {
    return std::stold(buffer);
}

std::string Lexer::get_string() const {
    return buffer;
}
