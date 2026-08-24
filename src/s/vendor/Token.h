// Tokens.
//
// Each token carries the line it started on. Newlines are whitespace to the
// lexer, so the stream for '? x ?? y' and for the same two commands on two
// lines is identical - nothing in the sequence tells them apart. The print
// rule needs to, so the line travels with the token. It is the sole piece of
// layout the language preserves.
#pragma once


#include <cstdint>
#include <string>
#include <vector>

namespace shalimar {

enum class Tok {
    IntLiteral,
    RealLiteral,
    StringLiteral,
    Identifier,

    // Arithmetic, comparison and logic all arrive as Operator with the
    // spelling in `text`, exactly as the app's lexer has them: the parser
    // reads the spelling, and keeping them one kind is what lets the
    // precedence tables be written as lists of spellings.
    Operator,

    Assign,        // :
    PlusAssign,    // +:
    MinusAssign,   // -:
    PrintLine,     // ?
    PrintInline,   // ??
    ParensOpen, ParensClose,
    BraceOpen, BraceClose,
    BracketOpen, BracketClose,
    Comma, Dot,

    If, ElseIf, Else, While, For, To, Step, Fun, Return,
    Break, Continue,
    Int, Real, Char,

    // Never produced by tokenize(). End of input is virtual: peek() past the
    // end manufactures one, which is what lets the parser report cleanly at
    // end of input instead of indexing out of bounds.
    EndOfInput
};

struct Token {
    Tok kind = Tok::EndOfInput;
    int line = 0;

    std::string text;      // Identifier, StringLiteral, Operator spelling
    int32_t intValue = 0;
    double realValue = 0.0;
};

// Tokenizing stops at the offending character, so the stream is truncated and
// nothing downstream is trustworthy - a lex error is reported alone.
struct LexResult {
    std::vector<Token> tokens;
    bool failed = false;
    int errorLine = 0;
    std::string error;
};

LexResult tokenize(const std::string& source);

// How a token is written in the source, for a diagnostic to quote back.
// EndOfInput has no spelling, and the one place that would need it says
// "Program ends unfinished" instead.
std::string spellingOf(const Token& token);

}  // namespace shalimar
