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
// converted, so nothing a header declares is wanted in the tree; and a
// directive that could change WHICH PROGRAM THIS IS - #if, #ifdef, #ifndef,
// #elif, #else, #endif, and any #define one of those tests - is reported as
// something to resolve by hand before conversion starts. Until it is
// resolved, what a parser would see is not what the author wrote:
//
//     #define TEST_VERSION
//     #ifdef TEST_VERSION
//     float test = 0.0f;
//     #else
//     double test = 0.0;
//     #endif
//
// There are two programs there and nothing in the file says which one is
// wanted. That is a question for the author, not a guess for a converter.
//
// A plain #define is a different thing entirely. '#define pi 3.14' does not
// branch the program - it names a value, and expanding it is exactly what a
// preprocessor would do and what the author means. Those are collected here
// and substituted into the token stream after lexing, so 'pi' reaches the
// parser as 3.14 with the offset of the place it was written. Asking about
// them was the old behaviour and it was wrong: it stopped a conversion to
// demand a hand edit that the converter could do itself, correctly.
//
// #undef, #pragma, #error and #line stay in the ask-first category. #undef
// changes what a name means partway down a file, and the other three are
// instructions to a compiler this converter is not.
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

    // One #define that does not decide anything, ready to substitute.
    struct Macro {
        std::string name;
        bool functionLike = false;         // '#define SQ(x)', not '#define N'
        std::vector<std::string> params;
        std::string body;                  // the replacement, as written
        int line = 0;
        std::size_t offset = 0;            // of the '#'
    };

    // Scans, reports, blanks. Returns false when a directive was found that
    // the author has to answer - the caller stops before parsing, because
    // those are PreprocessorFix diagnostics and the parse would be of a
    // program that has not been decided on.
    //
    // A #define that nothing tests is not one of those: it is collected into
    // macros() and the run carries on.
    bool run(const Source &source, Diagnostics &diagnostics);

    // The text to lex: the original with directive lines spaced out.
    const std::string &text() const { return text_; }

    const std::vector<Include> &includes() const { return includes_; }

    // The macros to substitute, in the order they were defined.
    const std::vector<Macro> &macros() const { return macros_; }

    // The directives awaiting a decision, as written, in file order - the
    // same set run() returned false for, in the form a caller shows the
    // user. The diagnostics say where each one is and what to do about it;
    // this is the list itself, for a caller with no console to print to.
    const std::vector<std::string> &pending() const { return pending_; }

private:
    std::string text_;
    std::vector<Include> includes_;
    std::vector<Macro> macros_;
    std::vector<std::string> pending_;
};

}  // namespace c2s

#endif
