#ifndef C2S_C_CTOKEN_H
#define C2S_C_CTOKEN_H

#include <cstddef>
#include <string>
#include <vector>

namespace c2s {

// One C89 token.
//
// The kind says which family it is in; the text is always the characters as
// written, so a diagnostic can quote the source and the printer can keep a
// literal's original spelling - 0x1F stays 0x1F in C-to-C printing and is
// refused with its own spelling in C-to-Shalimar.
enum class CTokenKind {
    End,
    Identifier,
    Keyword,
    IntLiteral,       // value and suffixes decoded alongside the text
    FloatLiteral,
    CharLiteral,      // value is the character's value
    StringLiteral,    // text is the decoded characters, spelling the original
    Punct
};

struct CToken {
    CTokenKind kind = CTokenKind::End;
    std::string text;          // decoded characters for a string, else spelling
    std::string spelling;      // exactly what the source said
    std::size_t offset = 0;    // byte offset into the scanned text

    // Numbers, decoded by the lexer so nothing downstream re-parses digits.
    long long intValue = 0;
    double floatValue = 0.0;
    bool isUnsigned = false;   // a U suffix
    bool isLong = false;       // an L suffix
    bool isFloatSuffix = false;  // an F suffix on a floating literal
    bool isHexOrOctal = false; // spelled in a base Shalimar does not have

    bool is(const char *s) const { return text == s; }
    bool isKeyword(const char *s) const { return kind == CTokenKind::Keyword && text == s; }
};

// What the lexer hands back: the tokens, or the reason it stopped.
struct CLexResult {
    std::vector<CToken> tokens;
    bool failed = false;
    std::size_t errorOffset = 0;
    std::string error;
};

}  // namespace c2s

#endif
