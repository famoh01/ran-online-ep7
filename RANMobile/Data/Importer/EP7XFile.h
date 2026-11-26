//
// EP7XFile.h — Shared DirectX .x (text) tokenizer for the EP7 character pipeline
//
// The original engine loads every character artefact through DirectX 9's
// IDirectXFile API:
//   DirectXFileCreate() + RegisterTemplates(D3DRM_XTEMPLATES) + CreateEnumObject()
//     enginelib/Meshs/DxBoneCollector.cpp   (skeleton  — Frame / FrameTransformMatrix)
//     enginelib/Meshs/DxSkinMeshLoad.cpp    (skin mesh — Mesh / SkinWeights / ...)
//     enginelib/Meshs/SAnimation.cpp        (animations authored in .x, then baked to .bin)
//
// This header provides the token stream those parsers need. It handles the standard
// text encoding only. The binary encodings (DXFILEFORMAT_BINARY "xof " and the
// compressed variant) are rejected — see IsBinaryX().
//
// Grammar subset (DirectX 9 template format):
//   file        := ( template-decl | data-object )*
//   template-decl := 'template' Name '{' GUID '}' ';'
//   data-object := Name [ Name | String ] '{' body '}'
//   body        := ( 'array' | Number | String | Identifier | data-object )*
//
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

namespace RANMobile {
namespace ep7xfile {

struct Token {
    enum class Kind { End, Identifier, Number, String, LBrace, RBrace, Semicolon, Comma, Other };

    Kind kind = Kind::End;
    std::string text;
    double number = 0.0;

    bool IsIdentifier(const char* name) const {
        return kind == Kind::Identifier && text == name;
    }
    bool IsNumber() const { return kind == Kind::Number; }
};

// A DirectX .x file always begins with a 16-byte header:
//   offset 0..3  "xof "
//   offset 4..7  version   e.g. "0303"
//   offset 8..11 format    "txt\0" | "bin\0" | "bzip" | "tzip"
//   offset 12..15 float size ("0032" / "0064")
// Both text and binary files therefore start with "xof ", so the format field at
// offset 8 — not the magic — decides the encoding. ("bzip"/"tzip" are the compressed
// binary variants, which D3DX reads but this reader does not.)
inline bool IsBinaryX(const uint8_t* data, size_t size) {
    if (!data || size < 12) return false;
    if (!(data[0] == 'x' && data[1] == 'o' && data[2] == 'f' && data[3] == 0x20)) return false;
    const uint8_t* format = data + 8;
    if (format[0] == 'b' && format[1] == 'i' && format[2] == 'n') return true;
    if (format[0] == 'b' && format[1] == 'z' && format[2] == 'i' && format[3] == 'p') return true;
    if (format[0] == 't' && format[1] == 'z' && format[2] == 'i' && format[3] == 'p') return true;
    return false;
}

// True when the 16-byte header declares the text encoding this reader supports.
inline bool IsTextX(const uint8_t* data, size_t size) {
    return size >= 12 && data[0] == 'x' && data[1] == 'o' && data[2] == 'f' && data[8] == 't' &&
           data[9] == 'x' && data[10] == 't';
}

// Full text tokenizer. Returns false only for binary input.
bool Tokenize(const uint8_t* data, size_t size, std::vector<Token>& out, std::string& error);

// Convenience cursor over a token vector.
class Cursor {
public:
    explicit Cursor(const std::vector<Token>& tokens) : m_tokens(tokens) {}

    bool AtEnd() const { return m_pos >= m_tokens.size(); }
    const Token& Peek() const {
        static const Token kEnd;
        return m_pos < m_tokens.size() ? m_tokens[m_pos] : kEnd;
    }
    const Token& Next() {
        const Token& t = Peek();
        if (m_pos < m_tokens.size()) ++m_pos;
        return t;
    }
    size_t Position() const { return m_pos; }
    void Seek(size_t pos) { m_pos = pos; }

    // Skips a balanced `{ ... }` block starting at the current '{'.
    bool SkipBlock() {
        if (Peek().kind != Token::Kind::LBrace) return false;
        int depth = 0;
        while (!AtEnd()) {
            const Token& t = Next();
            if (t.kind == Token::Kind::LBrace) ++depth;
            if (t.kind == Token::Kind::RBrace) {
                --depth;
                if (depth == 0) return true;
            }
        }
        return false;
    }

private:
    const std::vector<Token>& m_tokens;
    size_t m_pos = 0;
};

}  // namespace ep7xfile
}  // namespace RANMobile
