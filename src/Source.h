#ifndef C2S_SOURCE_H
#define C2S_SOURCE_H

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include "Location.h"

namespace c2s {

// A file could not be read. The only exception this converter throws for an
// input problem; everything a source file says about itself is a diagnostic
// instead, never an exception, because a conversion run reports all of them
// and then stops.
class SourceError : public std::runtime_error {
public:
    explicit SourceError(const std::string &what) : std::runtime_error(what) {}
};

// One source file, held whole, with the line index a diagnostic needs.
//
// Unlike Compiler-C's Source this holds the text the user wrote rather than
// the text a preprocessor produced, and it is never asked to fail: nothing
// here exits, throws on a syntax question, or writes to a stream. The line
// index is built once in the constructor rather than lazily, because a
// converter locates far more often than a compiler does and there is no
// thread that would race for it.
class Source {
public:
    Source(std::string name, std::string text);

    static Source fromFile(const std::string &path);

    const std::string &name() const { return name_; }
    const std::string &text() const { return text_; }
    std::size_t size() const { return text_.size(); }

    // 1-based, so lineCount() is the number of the last line.
    int lineCount() const { return static_cast<int>(lineStarts_.size()); }

    // The text of one line, without its terminator. Empty for a line number
    // outside the file, so a diagnostic that quotes a line it cannot find
    // prints nothing rather than crashing.
    std::string line(int lineNo) const;

    // A byte offset into text() becomes a place a person can point at.
    Location locate(std::size_t offset) const;

    // The offset a line begins at. lineCount() + 1 answers size().
    std::size_t offsetOfLine(int lineNo) const;

private:
    void indexLines();

    std::string name_;
    std::string text_;
    std::vector<std::size_t> lineStarts_;
};

}  // namespace c2s

#endif
