#ifndef C2S_C_CMACRO_H
#define C2S_C_CMACRO_H

#include <vector>

#include "CPreScan.h"
#include "CToken.h"

namespace c2s {

class Source;
class Diagnostics;

// Macro substitution, done on the token stream rather than on the text.
//
// CPreScan keeps the #defines that name a value rather than decide a
// program, and this puts them where they were used. Doing it here, after
// lexing, is what keeps a diagnostic honest: rewriting the text would move
// every byte after the first substitution, and the offsets a message quotes
// the source by would point at the wrong line for the rest of the file.
//
// So each token that comes out of a replacement carries the offset of the
// place the macro was WRITTEN, not of the replacement it came from - the
// replacement is not in the lexed text at all, its line having been blanked
// with every other directive. An argument's tokens keep their own offsets,
// because those the author really did write there.
//
// What is handled: object-like and function-like macros, arguments expanded
// before they are substituted, and the result rescanned so a macro may use
// another. A macro does not expand inside itself - C's rule, and the reason
// this terminates. Stringify and paste never arrive: CPreScan sends a
// replacement containing '#' back to the author instead.
//
// Returns false and reports when a call is malformed or the expansion will
// not settle; the tokens are left untouched in that case.
bool expandMacros(const std::vector<CPreScan::Macro> &macros,
                  std::vector<CToken> &tokens,
                  const Source &source, Diagnostics &diagnostics);

}  // namespace c2s

#endif
