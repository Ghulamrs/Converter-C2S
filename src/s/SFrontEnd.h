#ifndef C2S_S_SFRONTEND_H
#define C2S_S_SFRONTEND_H

#include <memory>
#include <string>

#include "vendor/Ast.h"
#include "vendor/Diag.h"

namespace c2s {

class Diagnostics;
class Source;

// Compiler-S's own lexer, parser and checker, driven as a library.
//
// The vendored front end is used exactly as shc uses it - tokenize, Parser,
// Checker, in that order - and its diagnostics are carried across into this
// converter's report afterwards, with the line numbers it recorded. Nothing
// in the vendored code exits or throws on a bad program; a parse that fails
// returns null and says why, which is the shape everything here relies on.
class SFrontEnd {
public:
    // Parse only. Null when the program did not parse; the reasons are in
    // diagnostics either way. A tree from this is printable but not typed.
    std::unique_ptr<shalimar::Program> parse(const Source &source,
                                             Diagnostics &diagnostics);

    // Parse and check. Null when either failed. A tree from this is typed:
    // every Expr::type() is set, conversions are explicit Convert nodes, and
    // symbols are resolved - which is what the Shalimar-to-C direction reads.
    std::unique_ptr<shalimar::Program> parseAndCheck(const Source &source,
                                                     Diagnostics &diagnostics);

private:
    void carryOver(const shalimar::Diagnostics &from, const Source &source,
                   Diagnostics &to) const;
};

}  // namespace c2s

#endif
