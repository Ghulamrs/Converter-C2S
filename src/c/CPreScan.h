#ifndef C2S_C_CPRESCAN_H
#define C2S_C_CPRESCAN_H

#include <string>
#include <vector>

namespace c2s {

class Source;
class Diagnostics;

// The preprocessor policy, applied to the raw text before anything lexes it.
//
// This converter does not run a preprocessor, on purpose. Headers are not
// converted, so nothing a header declares is wanted in the tree; and any
// directive that could change the program - #define, #if, #ifdef, #pragma
// and the rest - is reported as something to resolve by hand before
// conversion starts, because until it is resolved, what a parser would see
// is not what the author wrote. Only after preprocessing would those
// directives be gone, which is exactly why the scan happens before it.
//
// #include is the one directive that is accepted: it is recorded, dropped
// from the text, and never followed. What it would have declared does not
// matter to a parse - a C89 call does not need a prototype to be read, only
// to be compiled - and this converter's C parser is written for that.
//
// The scan hands back the text with every directive line blanked to spaces,
// so byte offsets and line numbers still point where they did in the file
// the user edited.
class CPreScan {
public:
    struct Include {
        std::string header;    // what was between the delimiters
        bool angled = false;   // <stdio.h> rather than "mine.h"
        int line = 0;
    };

    // Scans, reports, blanks. Returns false when a directive other than
    // #include was found - the caller stops before parsing, because those
    // are PreprocessorFix diagnostics and the parse would be of a program
    // the author has not finished deciding on.
    bool run(const Source &source, Diagnostics &diagnostics);

    // The text to lex: the original with directive lines spaced out.
    const std::string &text() const { return text_; }

    const std::vector<Include> &includes() const { return includes_; }

    // The directives awaiting a decision, as written, in file order - the
    // same set run() returned false for, in the form a caller shows the
    // user. The diagnostics say where each one is and what to do about it;
    // this is the list itself, for a caller with no console to print to.
    const std::vector<std::string> &pending() const { return pending_; }

private:
    std::string text_;
    std::vector<Include> includes_;
    std::vector<std::string> pending_;
};

}  // namespace c2s

#endif
