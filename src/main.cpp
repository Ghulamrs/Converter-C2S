#include <cstdio>
#include <fstream>
#include <iostream>
#include <ostream>
#include <string>
#include <vector>

#include "Converter.h"
#include "Diagnostics.h"
#include "Options.h"
#include "Source.h"

namespace {

const char *kVersion = "c2s 0.1";

// Exit statuses, chosen to match the two compilers this converter sits
// between: 0 for success, 1 for a program that was read and refused, 2 for a
// file that could not be read at all - or, at the other end, written.
const int kOk = 0;
const int kRefused = 1;
const int kCannotRead = 2;

void listCodes(std::ostream &out) {
    out << "Command line\n"
           "  C0001  unknown option\n"
           "  C0002  an option is missing its argument\n"
           "  C0003  more than one input file\n"
           "  C0004  no input file\n"
           "  C0005  the direction cannot be inferred from the extension\n";
}

// The preprocessor questions, printed as a list to work down rather than as
// diagnostics to read. The library returns them instead of guessing, and a
// console caller's whole job here is to put them in front of a person.
void writeQuestions(std::ostream &out, const std::string &name,
                    const std::vector<std::string> &questions) {
    out << name << ": " << questions.size() << " preprocessor construct"
        << (questions.size() == 1 ? "" : "s")
        << " must be resolved before conversion can start\n";
    for (std::size_t i = 0; i < questions.size(); ++i) {
        out << "  " << questions[i] << '\n';
    }
}

void writeDiagnostics(std::ostream &out,
                      const std::vector<c2s::Diagnostic> &diagnostics) {
    for (std::size_t i = 0; i < diagnostics.size(); ++i) {
        out << diagnostics[i].formatted() << '\n';
    }
}

// An empty path means standard output, which is what a converter reads like
// when it is one stage of a pipe.
bool writeOutput(const std::string &path, const std::string &text) {
    if (path.empty()) {
        std::cout << text;
        return std::cout.good();
    }
    std::ofstream out(path.c_str(), std::ios::binary);
    if (!out) return false;
    out << text;
    out.close();
    return out.good();
}

}  // namespace

int main(int argc, char **argv) {
    c2s::Diagnostics diagnostics;
    c2s::Options options;

    if (!options.parse(argc, argv, diagnostics)) {
        diagnostics.writeTo(std::cerr);
        std::cerr << c2s::Options::usage();
        return kRefused;
    }

    if (options.showHelp()) {
        std::cout << c2s::Options::usage();
        return kOk;
    }
    if (options.showVersion()) {
        std::cout << kVersion << '\n';
        return kOk;
    }
    if (options.listCodes()) {
        listCodes(std::cout);
        return kOk;
    }

    try {
        const c2s::Source source = c2s::Source::fromFile(options.input());

        // Everything below the library boundary is a pure function of the
        // text, so this is the whole of the tool: read a file, call it,
        // decide what the answer means for an exit status.
        const c2s::Converter::Result result =
            options.canonicalise()
                ? c2s::Converter::canonicalise(source.text(), source.name(),
                                               options.resolvedDirection())
                : c2s::Converter::convert(source.text(), source.name(),
                                          options.resolvedDirection(),
                                          options.permissions(),
                                          options.emitIncludes());

        // The questions come first and stand alone. Until they are answered
        // the parse below them was of a program the author has not finished
        // writing, so its diagnostics would be about the wrong file.
        if (!result.questions.empty()) {
            writeQuestions(std::cerr, source.name(), result.questions);
        }
        writeDiagnostics(std::cerr, result.diagnostics);
        if (!result.diagnostics.empty() && !result.summary.empty()) {
            std::cerr << result.summary << '\n';
        }

        // Nothing is written while an error is outstanding. A converter that
        // half-writes a file is worse than one that refuses, because the
        // half-written one may still compile.
        if (!result.ok) return kRefused;

        if (!writeOutput(options.output(), result.output)) {
            std::cerr << "c2s: cannot write '"
                      << (options.output().empty() ? "-" : options.output())
                      << "'\n";
            return kCannotRead;
        }

        // Markers are written and then refused. The output exists to be read
        // and finished by hand; what it is not yet is a program, and an exit
        // status of 0 would say that it was.
        if (result.beyondCount > 0) {
            std::cerr << source.name() << ": " << result.beyondCount
                      << (result.beyondCount == 1 ? " construct has"
                                                  : " constructs have")
                      << " no expression in the target language, and each is"
                         " marked where it stands in the output\n";
            return kRefused;
        }

        return kOk;
    } catch (const c2s::SourceError &error) {
        std::cerr << "c2s: " << error.what() << '\n';
        return kCannotRead;
    }
}
