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

}

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

        const c2s::Converter::Result result =
            options.canonicalise()
                ? c2s::Converter::canonicalise(source.text(), source.name(),
                                               options.resolvedDirection())
                : c2s::Converter::convert(source.text(), source.name(),
                                          options.resolvedDirection(),
                                          options.permissions(),
                                          options.emitIncludes());

        if (!result.questions.empty()) {
            writeQuestions(std::cerr, source.name(), result.questions);
        }
        writeDiagnostics(std::cerr, result.diagnostics);
        if (!result.diagnostics.empty() && !result.summary.empty()) {
            std::cerr << result.summary << '\n';
        }

        if (!result.ok) return kRefused;

        if (!writeOutput(options.output(), result.output)) {
            std::cerr << "c2s: cannot write '"
                      << (options.output().empty() ? "-" : options.output())
                      << "'\n";
            return kCannotRead;
        }

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
