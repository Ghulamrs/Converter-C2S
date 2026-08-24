#ifndef C2S_LOCATION_H
#define C2S_LOCATION_H

#include <string>

namespace c2s {

// Where something is, in the file a person edited.
//
// Both compilers this converter answers to record position differently -
// Compiler-C keeps a byte offset into the preprocessed text and resolves it
// through Source::locate, Compiler-S keeps a line number and nothing else.
// A converter has to quote the offending construct back, so it keeps the
// column too, and keeps it from the file as written rather than from anything
// a preprocessor produced.
class Location {
public:
    Location() : line_(0), column_(0) {}
    Location(std::string file, int line, int column)
        : file_(std::move(file)), line_(line), column_(column) {}

    const std::string &file() const { return file_; }
    int line() const { return line_; }
    int column() const { return column_; }

    bool isKnown() const { return line_ > 0; }

    // "prime.c:14:9", or "prime.c:14" when the column is not known, or
    // "prime.c" when neither is.
    std::string spelling() const;

private:
    std::string file_;
    int line_;
    int column_;
};

}  // namespace c2s

#endif
