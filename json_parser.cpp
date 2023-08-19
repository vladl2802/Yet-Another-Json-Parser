//
// Created by Vlad on 09.07.2023.
//

#include "json_parser.h"

JsonParser::ParserUnit::Vertex*
JsonParser::ParserUnit::create_vertex(JsonParser::StructureToken type, std::unique_ptr<BasicParserCallback> callback) {
    if (callback != nullptr) assert(type == StructureToken::callback); // TODO: replace with exception
    switch (type) {
        case StructureToken::array:
            return new ArrayVertex(type, nullptr, {}, 0);
        case StructureToken::object:
            return new ObjectVertex(type, nullptr, {}, "");
        case StructureToken::callback:
            return new CallbackVertex(type, nullptr, std::move(callback));
        case StructureToken::unknown:
            return new Vertex(type, nullptr);
        default:
            assert(false); // TODO: replace with exception
    }
}

void JsonParser::ParserUnit::add_callback_listener(std::unique_ptr<BasicParserCallback> callback, std::initializer_list<ParserVariablePath> path) {
    static_assert(!std::is_abstract_v<decltype(*callback)>); // callback must be final
    Vertex** cur = &vertex_stack[0];
    for (const auto& path_element : path) {
        assert(cur != nullptr); // TODO: replace with exception
        StructureToken type;
        if (std::holds_alternative<uint32_t>(path_element)) type = StructureToken::array;
        else if (std::holds_alternative<std::string>(path_element) || std::holds_alternative<const char*>(path_element)) type = StructureToken::object;
        else if (std::holds_alternative<StructureParsingFlag>(path_element)) type = StructureToken::unknown;
        else assert(false); // TODO: replace with exception

        if (*cur == nullptr) {
            *cur = create_vertex(type, nullptr);
        } else if ((*cur)->type == StructureToken::unknown && type != StructureToken::unknown) {
            auto temp = create_vertex(type, nullptr);
            std::swap(temp->go_for_each, (*cur)->go_for_each);
            std::swap(temp, *cur);
            delete temp;
        }

        static auto jump = []<typename T>(NestingVertex<T>* const vertex, const T& name) -> Vertex** {
            return &vertex->to_go[name];
        };

        switch (type) {
            case StructureToken::array:
                cur = jump(dynamic_cast<ArrayVertex*>(*cur), std::get<uint32_t>(path_element));
                break;
            case StructureToken::object: {
                std::string name;
                if (std::holds_alternative<const char *>(path_element)) name = std::string{get<const char *>(path_element)};
                else name = get<std::string>(path_element);
                cur = jump(dynamic_cast<ObjectVertex *>(*cur), name);
                break;
            } case StructureToken::unknown:
                cur = &((*cur)->go_for_each);
                break;
            default:
                assert(false); // TODO: replace with exception
        }
    }
    assert(*cur == nullptr); // TODO: replace with exception
    *cur = create_vertex(StructureToken::callback, std::move(callback));
}

void JsonParser::ParserUnit::scan() {
    static auto token_to_structure = [](Token tk) -> StructureToken {
        switch (tk) {
            case Token::begin_array: [[fallthrough]];
            case Token::end_array:
                return StructureToken::array;
            case Token::begin_object: [[fallthrough]];
            case Token::end_object:
                return StructureToken::object;
            default:
                assert(false);
        }
    };


    bool to_ignore = false;
    uint16_t ignore_depth = 0;
    Token tk;
    std::vector<StructureToken> structure_stack;
    while (!vertex_stack.empty()) {
        auto& cur = vertex_stack.back();
        bool start_nesting = false;
        if (to_ignore) {
            tk = parent.expect_token(TokenPattern::value);
            if (check_pattern(tk, TokenPattern::begin)) {
                structure_stack.push_back(token_to_structure(tk));
                ignore_depth++;
                start_nesting = true;
            } else {
                if (ignore_depth == 0) to_ignore = false;
            }
        } else {
            switch (cur->type) {
                case StructureToken::callback: {
                    auto pu = new ParserUnit(parent);
                    auto ver = dynamic_cast<CallbackVertex*>(cur);
                    parent.units.push_back(pu);
                    ver->callback->invoke(pu, this);
                    parent.units.pop_back();
                    delete pu;

                    vertex_stack.pop_back();
                    cur = vertex_stack.back();
                    break;
                } case StructureToken::array:
                    start_nesting = true;
                    parent.expect_token(Token::begin_array);
                    structure_stack.push_back(StructureToken::array);
                    break;
                case StructureToken::object:
                    start_nesting = true;
                    parent.expect_token(Token::begin_object);
                    structure_stack.push_back(StructureToken::object);
                    break;
                default:
                    assert(false); // TODO: replace with exception
            }
        }

        if (!start_nesting) {
            while (!vertex_stack.empty()) {
                tk = parent.expect_token(std::to_underlying(TokenPattern::end) | std::to_underlying(Token::element_delimiter));
                if (tk == Token::element_delimiter) break;
                else {
                    assert(structure_stack.back() == token_to_structure(tk)); // TODO: replace with exception
                    structure_stack.pop_back();
                    if (to_ignore) {
                        ignore_depth--;
                        if (ignore_depth == 0) to_ignore = false;
                    } else {
                        vertex_stack.pop_back();
                        cur = vertex_stack.back();
                    }
                }
            }
            if (vertex_stack.empty()) break;
        }

        static auto jump = []<typename T>(NestingVertex<T>* const ver, const T& name) -> Vertex*& {
            if (ver->to_go.find(name) == ver->to_go.end()) return ver->go_for_each;
            else return ver->to_go[name];
        };

        switch (cur->type) {
            case StructureToken::array: {
                auto ver = dynamic_cast<ArrayVertex*>(vertex_stack.back());
                auto& index = ver->last;

                Vertex* result = jump(ver, index);
                if (result == nullptr) to_ignore = true;
                else {
                    ver->last = index;
                    vertex_stack.push_back(result);
                }
                index++;
                break;
            } case StructureToken::object: {
                auto ver = dynamic_cast<ObjectVertex*>(vertex_stack.back());
                parent.expect_token(Token::string, true);
                auto name = parent.lexer.get_string();
                parent.expect_token(Token::name_delimiter);

                Vertex* result = jump(ver, name);
                if (result == nullptr) to_ignore = true;
                else {
                    ver->last = name;
                    vertex_stack.push_back(result);
                }
                break;
            } default:
                assert(false); // TODO: replace with exception
        }
    }
}

JsonParser::ParserUnit::ParserUnit(JsonParser& parent) : parent(parent) {
    auto root = new ObjectVertex(StructureToken::object, nullptr, {}, "");
    vertex_stack.push_back(root);
}

JsonParser::Token JsonParser::scan_token(bool memorise) {
    return lexer.scan_token(memorise);
}

JsonParser::Token JsonParser::expect_token(Token tk, bool memorise) {
    auto res = lexer.scan_token(memorise);
    if (tk == res) return res;
    else assert(false); // TODO: replace with exception
}

JsonParser::Token JsonParser::expect_token(uint16_t tk, bool memorise) {
    auto res = lexer.scan_token(memorise);
    if (tk & std::to_underlying(res)) return res;
    else assert(false); // TODO: replace with exception
}

JsonParser::Token JsonParser::expect_token(TokenPattern tk_pat, bool memorise) {
    auto res = lexer.scan_token(memorise);
    if (check_pattern(res, tk_pat)) return res;
    else assert(false); // TODO: replace with exception
}

bool JsonParser::check_pattern(JsonParser::Token tk, JsonParser::TokenPattern pat) {
    return std::to_underlying(pat) & std::to_underlying(tk);
}

JsonParser::JsonParser(IStreamWrapper&& is) : lexer(std::move(is)) {
    auto base = new ParserUnit(*this);
    units.push_back(base);
}

JsonParser::ParserUnit *const JsonParser::base_parser_unit() {
    return units[0];
}

JsonParser::ParserUnit::Vertex::Vertex(JsonParser::StructureToken type, JsonParser::ParserUnit::Vertex *go_for_each)
                                      : type(type), go_for_each(go_for_each) {}

JsonParser::ParserUnit::CallbackVertex::CallbackVertex(JsonParser::StructureToken type, JsonParser::ParserUnit::Vertex *go_for_each,
                                                       std::unique_ptr<BasicParserCallback> callback)
                                                       : Vertex(type, go_for_each), callback(std::move(callback)) {}
