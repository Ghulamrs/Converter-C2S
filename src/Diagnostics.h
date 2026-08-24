#ifndef C2S_DIAGNOSTICS_H
#define C2S_DIAGNOSTICS_H

#include <iosfwd>
#include <string>
#include <vector>

#include "Location.h"

namespace c2s {

class Source;

// What kind of thing a message is.
//
// The two error kinds are deliberately separate. A SyntaxError says the input
// is not a valid program in its own language and the user should take it to
// cc1 or shc; a ConversionError says the input is perfectly good and the
// target language has nowhere to put it. They are read differently and they
// are fixed differently, so they are counted differently.
enum class Severity {
    Note,             // context hung off the message above it
    Warning,          // converted, but something a reader should know shifted
    SyntaxError,      // not a valid program in the language it was written in
    ConversionError,  // valid, but it has no expression in the other language
    PreprocessorFix   // must be resolved by hand before conversion can start
};

const char *spellingOf(Severity severity);

// One message. Built whole, then handed to Diagnostics; there is no way to
// reach back into a report and edit a message already in it.
class Diagnostic {
public:
    Diagnostic(Severity severity, Location where, std::string code, std::string message)
        : severity_(severity),
          where_(std::move(where)),
          code_(std::move(code)),
          message_(std::move(message)) {}

    Severity severity() const { return severity_; }
    const Location &where() const { return where_; }
    const std::string &code() const { return code_; }
    const std::string &message() const { return message_; }
    const std::string &snippet() const { return snippet_; }
    const std::string &hint() const { return hint_; }

    void setSnippet(std::string text) { snippet_ = std::move(text); }
    void setHint(std::string text) { hint_ = std::move(text); }

    bool isError() const {
        return severity_ == Severity::SyntaxError ||
               severity_ == Severity::ConversionError ||
               severity_ == Severity::PreprocessorFix;
    }

    // Several lines: the header, the quoted source line with a caret under
    // the column, and the hint. A message with no snippet prints one line.
    std::string formatted() const;

private:
    Severity severity_;
    Location where_;
    std::string code_;
    std::string message_;
    std::string snippet_;
    std::string hint_;
};

// Every message a run produced, in the order they were found.
//
// Nothing here stops a run. The converter walks the whole tree, collects every
// refusal it meets, and the caller decides at the end whether an output file
// is written - which is what makes one pass over a file enough to fix it,
// rather than one pass per mistake.
class Diagnostics {
public:
    Diagnostics() : errors_(0), warnings_(0) {}

    void add(Diagnostic diagnostic);

    // The common shape: the snippet and the caret come from the source.
    void report(Severity severity, const Source &source, const Location &where,
                const std::string &code, const std::string &message,
                const std::string &hint = std::string());

    // For a message with no file behind it - a command line problem, or a
    // rule about the program as a whole.
    void report(Severity severity, const std::string &code, const std::string &message);

    bool hasErrors() const { return errors_ > 0; }
    int errorCount() const { return errors_; }
    int warningCount() const { return warnings_; }
    bool empty() const { return messages_.empty(); }

    // True when at least one message is a PreprocessorFix. Those are answered
    // before anything else, because until they are the parse means nothing.
    bool hasPreprocessorFixes() const;

    const std::vector<Diagnostic> &messages() const { return messages_; }

    void writeTo(std::ostream &out) const;

    // "3 conversion errors, 1 warning" - the last line of a run.
    std::string summary() const;

private:
    std::vector<Diagnostic> messages_;
    int errors_;
    int warnings_;
};

}  // namespace c2s

#endif
