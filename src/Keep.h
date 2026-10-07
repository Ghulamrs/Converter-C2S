// Keep: each top-level construct of the input paired with what it became, written to
// `.c2s-original/<output>`, so that converting back writes an unchanged construct as its
// original text, converts an edited or added one fresh, and drops a deleted one.
#ifndef C2S_KEEP_H
#define C2S_KEEP_H

#include <functional>
#include <string>
#include <vector>

namespace c2s {

// One top-level construct: its leading blank lines and comments, then its body.
struct Chunk {
    enum Kind { Code, Directive, Beyond, Trailer };
    Kind kind = Code;
    std::string text;
    std::string key;
};

// Cuts a C or Shalimar file into top-level constructs; the cut is purely textual,
// so the same text always cuts the same way and the pieces concatenate to the whole.
class Segmenter {
public:
    static std::vector<Chunk> split(const std::string &text);
    static std::vector<std::string> identifiers(const std::string &text);
    // The original lines a #BEYOND SHALIMAR comment block quotes, or false if it is not one.
    static bool beyondLines(const std::string &block, std::vector<std::string> &lines);
};

// The sidecar: for each construct, the original text and the text it became.
class Sidecar {
public:
    struct Segment {
        std::string source;
        std::string output;
        std::string key;
    };

    std::string from;
    std::string to;
    std::string sourceName;
    std::vector<Segment> segments;

    static Sidecar build(const std::string &source, const std::string &output,
                         const std::string &from, const std::string &to,
                         const std::string &sourceName);
    std::string serialise() const;
    static bool parse(const std::string &text, Sidecar &out);

    std::string output() const;
    std::string source() const;
};

// Restoring: the edited output, the sidecar, and a fresh conversion of the edited
// output on demand (called only where something has changed).
class Keeper {
public:
    struct Restored {
        bool usable = false;
        std::string text;
        int kept = 0;
        int fresh = 0;
        int dropped = 0;
        std::string note;
    };

    static Restored restore(const Sidecar &sidecar, const std::string &edited,
                            const std::function<bool(std::string &)> &convertFresh);

    // Without a sidecar: top-level #BEYOND SHALIMAR blocks of `input` written back
    // as their original lines, each before the construct that follows it.
    static std::string restoreBeyond(const std::string &input, const std::string &converted,
                                     int &restored);
};

}

#endif
