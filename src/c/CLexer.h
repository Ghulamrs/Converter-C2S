#ifndef C2S_C_CLEXER_H
#define C2S_C_CLEXER_H

#include <string>

#include "CToken.h"

namespace c2s {

// C89 text to tokens.
//
// The grammar is C90's, matching what Compiler-C reads, with the same two
// deliberate refusals: no trigraphs and no wide literals reach the converter
// meaningfully (a wide literal lexes, and the conversion pass refuses it with
// a location rather than the lexer guessing). Comments of both styles are
// eaten as whitespace. Every token keeps its original spelling, which is what
// lets the C printer put back '0x1F' rather than '31' and lets a diagnostic
// quote the file rather than a paraphrase.
class CLexer {
public:
    explicit CLexer(const std::string &text) : text_(text) {}

    CLexResult tokenize();

private:
    bool step(CLexResult &result);
    bool number(CLexResult &result);
    bool charLiteral(CLexResult &result);
    bool stringLiteral(CLexResult &result);
    bool punct(CLexResult &result);
    bool escape(long long *value, std::string *spelling, CLexResult &result);
    bool fail(CLexResult &result, const std::string &message);

    char at(std::size_t i) const { return i < text_.size() ? text_[i] : '\0'; }

    const std::string &text_;
    std::size_t i_ = 0;
    std::size_t tokenStart_ = 0;
};

}  // namespace c2s

#endif
