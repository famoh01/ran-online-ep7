#include "EP7XFile.h"

namespace RANMobile {
namespace ep7xfile {
namespace {

bool IsIdentifierStart(uint8_t c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}
bool IsIdentifierBody(uint8_t c) {
    return IsIdentifierStart(c) || (c >= '0' && c <= '9');
}
bool IsNumberStart(uint8_t c) {
    return (c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.';
}
bool IsNumberBody(uint8_t c) {
    return (c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E';
}

}  // namespace

bool Tokenize(const uint8_t* data, size_t size, std::vector<Token>& out, std::string& error) {
    out.clear();
    error.clear();

    if (!data || size == 0) {
        error = "empty .x buffer";
        return false;
    }
    if (IsBinaryX(data, size)) {
        error = "binary .x (xof) files are not supported by this reader";
        return false;
    }

    size_t pos = 0;
    while (pos < size) {
        const uint8_t c = data[pos];

        // whitespace
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            ++pos;
            continue;
        }
        // // line comment
        if (c == '/' && pos + 1 < size && data[pos + 1] == '/') {
            while (pos < size && data[pos] != '\n') ++pos;
            continue;
        }
        // # line comment
        if (c == '#') {
            while (pos < size && data[pos] != '\n') ++pos;
            continue;
        }

        Token tok;
        switch (c) {
            case '{': tok.kind = Token::Kind::LBrace; ++pos; break;
            case '}': tok.kind = Token::Kind::RBrace; ++pos; break;
            case ';': tok.kind = Token::Kind::Semicolon; ++pos; break;
            case ',': tok.kind = Token::Kind::Comma; ++pos; break;
            default: tok.kind = Token::Kind::Other; break;
        }
        if (tok.kind != Token::Kind::Other) {
            out.push_back(std::move(tok));
            continue;
        }

        if (c == '"') {
            ++pos;
            std::string s;
            while (pos < size && data[pos] != '"') s.push_back(static_cast<char>(data[pos++]));
            if (pos < size) ++pos;
            tok.kind = Token::Kind::String;
            tok.text = std::move(s);
            out.push_back(std::move(tok));
            continue;
        }

        if (IsIdentifierStart(c)) {
            const size_t start = pos;
            while (pos < size && IsIdentifierBody(data[pos])) ++pos;
            tok.kind = Token::Kind::Identifier;
            tok.text.assign(reinterpret_cast<const char*>(data + start), pos - start);
            out.push_back(std::move(tok));
            continue;
        }

        if (IsNumberStart(c)) {
            const size_t start = pos;
            while (pos < size && IsNumberBody(data[pos])) ++pos;
            tok.kind = Token::Kind::Number;
            tok.text.assign(reinterpret_cast<const char*>(data + start), pos - start);
            tok.number = std::strtod(tok.text.c_str(), nullptr);
            out.push_back(std::move(tok));
            continue;
        }

        // Anything else (GUID dashes, dots, ':' ...) becomes a single Other token.
        tok.kind = Token::Kind::Other;
        tok.text.assign(1, static_cast<char>(c));
        out.push_back(std::move(tok));
        ++pos;
    }

    return true;
}

}  // namespace ep7xfile
}  // namespace RANMobile
