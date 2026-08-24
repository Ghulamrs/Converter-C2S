#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

#include "Diagnostics.h"
#include "Options.h"
#include "Source.h"

namespace {

const char *kVersion = "c2s 0.1";

// Exit statuses, chosen to match the two compilers this converter sits
// between: 0 for success, 1 for a program that was read and refused, 2 for a
// file that could not be read at all.
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

        // The two pipelines land here as they are written. Until then a run
        // says so rather than writing an empty file, which is the one outcome
        // a converter must never produce quietly.
        std::cerr << source.name() << ": the conversion pipeline is not wired up yet\n";
        return kRefused;
    } catch (const c2s::SourceError &error) {
        std::cerr << "c2s: " << error.what() << '\n';
        return kCannotRead;
    }
}
