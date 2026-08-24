// Diagnostics.
//
// Every message names its line and takes one of two forms:
//
//     Error: line 3: ...          the program does not run
//     Warning: line 3: ...        the program runs anyway
//
// There is deliberately no stage name in the text. Which stage caught a
// problem is an implementation detail, and the vocabulary of a compiler's
// internals has no place in a message aimed at someone writing a program.
// For the same reason nothing here quotes a token kind or an AST node type.
//
// Messages are kept short on purpose: the app's console line holds about 47
// characters, and this compiler answers to the same document the app does.
#pragma once


#include <string>
#include <vector>

namespace shalimar {

enum class Severity { Error, Warning };

struct Message {
    Severity severity;
    int line;                   // 0 means the program as a whole
    std::string text;
    // Which source file it is in. Unit 0 is the program's own, and is not
    // named: 'Error: line 3: ...' is what the app prints and what every
    // single-file program must keep on printing. A file the compiler went
    // looking in is named, because otherwise the line number points at a file
    // the reader is not looking at.
    int unit = 0;

    std::string formatted(const std::vector<std::string> &units) const;
};

// The checker does not stop at the first problem, so diagnostics accumulate
// rather than throw. Errors and warnings share one list and one order: they
// are reported in the order they were found, not sorted by severity.
class Diagnostics {
public:
    void error(int line, const std::string& text);
    void warning(int line, const std::string& text);
    void error(int unit, int line, const std::string& text);
    void warning(int unit, int line, const std::string& text);

    // The files, in the order they were given numbers. Unit 0 is the program.
    void nameUnits(const std::vector<std::string>& names) { units_ = names; }

    // Something the language has and this compiler has not reached yet.
    //
    // It is kept apart from an error on purpose. An error is a statement
    // about the program; this is a statement about the compiler, and saying
    // so in the vocabulary of a language diagnostic would teach the reader
    // that their correct program is wrong. It goes to standard error, names
    // itself as the compiler speaking, and exits with a status of its own.
    void unsupported(int line, const std::string& what);

    bool hasUnsupported() const { return !unsupported_.empty(); }
    const std::vector<Message>& unsupportedItems() const { return unsupported_; }

    bool hasErrors() const { return errors_ > 0; }
    const std::vector<Message>& messages() const { return messages_; }
    void writeTo(std::string& out) const;

private:
    std::vector<Message> messages_;
    std::vector<Message> unsupported_;
    std::vector<std::string> units_;
    int errors_ = 0;
};

}  // namespace shalimar
