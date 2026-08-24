#ifndef C2S_OPTIONS_H
#define C2S_OPTIONS_H

#include <string>
#include <vector>

namespace c2s {

class Diagnostics;

// Which way round a run goes. Inferred from the input's extension unless the
// command line says otherwise: .c becomes Shalimar, .shm and .shl become C89.
enum class Direction { Infer, CToShalimar, ShalimarToC };

// The rewrites that are refused by default.
//
// Each of these is a place where a mechanical translation compiles but does
// not mean the same thing, so each is named on the command line rather than
// hidden behind one switch. --pragmatic turns on all of them at once and is
// the only shorthand.
class Permissions {
public:
    Permissions()
        : shortCircuit_(false), fallThrough_(false), charArithmetic_(false),
          narrowing_(false) {}

    // '&&' and '||' become nested ifs with a lifted temporary, because
    // Shalimar's '&' and '|' evaluate both sides before either is asked.
    bool shortCircuit() const { return shortCircuit_; }
    void allowShortCircuit() { shortCircuit_ = true; }

    // A switch case that falls into the next one becomes a 'matched' flag and
    // a chain of tests rather than a conversion error.
    bool fallThrough() const { return fallThrough_; }
    void allowFallThrough() { fallThrough_ = true; }

    // int() is inserted around a char used in arithmetic - c - '0' becomes
    // int(c) - 48 - rather than the char reaching an operator that refuses it.
    bool charArithmetic() const { return charArithmetic_; }
    void allowCharArithmetic() { charArithmetic_ = true; }

    // A C type wider than Shalimar's - long, unsigned, float - is narrowed to
    // int or real instead of being refused.
    bool narrowing() const { return narrowing_; }
    void allowNarrowing() { narrowing_ = true; }

    void allowEverything() {
        shortCircuit_ = true;
        fallThrough_ = true;
        charArithmetic_ = true;
        narrowing_ = true;
    }

private:
    bool shortCircuit_;
    bool fallThrough_;
    bool charArithmetic_;
    bool narrowing_;
};

// What one invocation was asked to do.
class Options {
public:
    Options() : direction_(Direction::Infer), emitIncludes_(true),
                showHelp_(false), showVersion_(false), listCodes_(false) {}

    // Reads argv. Anything wrong is a diagnostic rather than a throw or an
    // exit, so a caller can print the whole command line's worth of mistakes
    // at once. Returns false when the run should not proceed.
    bool parse(int argc, char **argv, Diagnostics &diagnostics);

    const std::string &input() const { return input_; }
    const std::string &output() const { return output_; }
    Direction direction() const { return direction_; }
    const std::vector<std::string> &includePath() const { return includePath_; }
    const Permissions &permissions() const { return permissions_; }

    // Generated C carries the #include lines its output needs - <math.h> for
    // pow, <stdio.h> for printf - because otherwise it does not compile. The
    // rule about not converting headers is about the input side.
    bool emitIncludes() const { return emitIncludes_; }

    bool showHelp() const { return showHelp_; }
    bool showVersion() const { return showVersion_; }
    bool listCodes() const { return listCodes_; }

    // The direction actually taken, with Infer resolved against the input's
    // extension. Direction::Infer is returned when the extension says nothing.
    Direction resolvedDirection() const;

    static const char *usage();

private:
    std::string input_;
    std::string output_;
    Direction direction_;
    std::vector<std::string> includePath_;
    Permissions permissions_;
    bool emitIncludes_;
    bool showHelp_;
    bool showVersion_;
    bool listCodes_;
};

}  // namespace c2s

#endif
