#include <cstdio>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#endif
#include <fstream>
#include <iostream>
#include <ostream>
#include <string>
#include <vector>

#include "Converter.h"
#include "Diagnostics.h"
#include "Keep.h"
#include "KeepCheck.h"
#include "Options.h"
#include "Source.h"

namespace {

// The family's banner, numbered with the group rather than on its own: c2s runs a particular cc1 and shc as oracles, so its number moves with theirs (CLAUDE.md, "The version number").
const char *kVersion = "\xC2\xA9""2026 G. R. Akhtar - C and Shalimar, either way 1.2";

const int kOk = 0;
const int kRefused = 1;
const int kCannotRead = 2;

void listCodes(std::ostream &out) {
    out << "Command line\n"
           "  C0001  unknown option\n"
           "  C0002  an option is missing its argument\n"
           "  C0003  more than one input file, or a pattern matching more than one\n"
           "  C0004  no input file\n"
           "  C0005  the direction cannot be inferred from the extension\n"
           "  C0006  a pattern with * or ? matches no file\n";
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

// One `output-line: input-line` pair per line of the output, on standard error so that `-o -` stays usable and the converted text is never interleaved with anything else.
// An output line no construct owns is written with a `-`, which is a thing to see rather than a 0 to interpret.
void writeLineMap(std::ostream &out, const std::vector<int> &map) {
    for (std::size_t i = 0; i < map.size(); ++i) {
        out << (i + 1) << ": ";
        if (map[i] > 0) out << map[i]; else out << '-';
        out << '\n';
    }
}

bool readFile(const std::string &path, std::string &text) {
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in) return false;
    text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

// The record of a converted file: `.c2s-original/<its name>` in its own directory.
std::string recordPath(const std::string &file, std::string *folder = nullptr) {
    const std::size_t slash = file.find_last_of("/\\");
    const std::string dir = slash == std::string::npos ? "" : file.substr(0, slash + 1);
    const std::string base = slash == std::string::npos ? file : file.substr(slash + 1);
    if (folder) *folder = dir + ".c2s-original";
    return dir + ".c2s-original/" + base;
}

bool writeRecord(const std::string &output, const std::string &text) {
    std::string folder;
    const std::string path = recordPath(output, &folder);
#ifdef _WIN32
    _mkdir(folder.c_str());
    SetFileAttributesA(folder.c_str(), FILE_ATTRIBUTE_HIDDEN);
#else
    mkdir(folder.c_str(), 0777);
#endif
    std::ofstream out(path.c_str(), std::ios::binary);
    out << text;
    out.close();
    return out.good();
}

int markers(const std::string &text) {
    int n = 0;
    for (std::size_t p = text.find("#BEYOND SHALIMAR"); p != std::string::npos;
         p = text.find("#BEYOND SHALIMAR", p + 1)) ++n;
    return n;
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

        const c2s::Direction direction = options.resolvedDirection();
        const bool toS = direction == c2s::Direction::CToShalimar;
        if (options.keepCheck()) {
            return c2s::KeepCheck::run(source.text(), source.name(), direction,
                                       options.permissions());
        }

        // A sidecar beside the input from the conversion that made it: unchanged constructs go back as they were.
        if (!options.canonicalise() && !options.fresh()) {
            std::string kept;
            c2s::Sidecar side;
            if (readFile(recordPath(options.input()), kept)) {
                const std::string from = toS ? "c" : "shalimar", to = toS ? "shalimar" : "c";
                if (!c2s::Sidecar::parse(kept, side) || side.from != to || side.to != from) {
                    std::cerr << "c2s: note: " << recordPath(options.input())
                              << " is not this file's record, and is ignored\n";
                } else {
                    c2s::Converter::Result freshResult;
                    auto fresh = [&](std::string &out) {
                        freshResult = c2s::Converter::convert(source.text(), source.name(), direction,
                                                              options.permissions(),
                                                              options.emitIncludes());
                        out = freshResult.output;
                        return freshResult.ok;
                    };
                    const c2s::Keeper::Restored r = c2s::Keeper::restore(side, source.text(), fresh);
                    if (r.usable) {
                        writeDiagnostics(std::cerr, freshResult.diagnostics);
                        if (!writeOutput(options.output(), r.text)) {
                            std::cerr << "c2s: cannot write '" << options.output() << "'\n";
                            return kCannotRead;
                        }
                        if (!options.output().empty() && options.output() != "-" && !options.noKeep()) {
                            writeRecord(options.output(),
                                        c2s::Sidecar::build(source.text(), r.text, from, to,
                                                            source.name()).serialise());
                        }
                        const int left = markers(r.text);
                        if (left > 0) {
                            std::cerr << source.name() << ": " << left
                                      << (left == 1 ? " construct has" : " constructs have")
                                      << " no expression in the target language, and each is"
                                         " marked where it stands in the output\n";
                            return kRefused;
                        }
                        return kOk;
                    }
                    if (!r.note.empty()) std::cerr << "c2s: note: " << r.note << '\n';
                }
            }
        }

        c2s::Converter::Result result =
            options.canonicalise()
                ? c2s::Converter::canonicalise(source.text(), source.name(),
                                               options.resolvedDirection())
                : c2s::Converter::convert(source.text(), source.name(),
                                          direction,
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

        if (options.showLineMap()) writeLineMap(std::cerr, result.lineMap);

        // No sidecar: a #BEYOND SHALIMAR block at the top level quotes the target language, and goes back as it was.
        if (!options.canonicalise()) {
            int restored = 0;
            result.output = c2s::Keeper::restoreBeyond(source.text(), result.output, restored);
            if (restored > 0) {
                std::cerr << "c2s: note: " << restored << " #BEYOND SHALIMAR block"
                          << (restored == 1 ? "" : "s") << " restored to the original lines\n";
            }
        }

        if (!writeOutput(options.output(), result.output)) {
            std::cerr << "c2s: cannot write '"
                      << (options.output().empty() ? "-" : options.output())
                      << "'\n";
            return kCannotRead;
        }

        if (!options.canonicalise() && !options.noKeep() && !options.output().empty() &&
            options.output() != "-") {
            const bool cToS = options.resolvedDirection() == c2s::Direction::CToShalimar;
            writeRecord(options.output(),
                        c2s::Sidecar::build(source.text(), result.output, cToS ? "c" : "shalimar",
                                            cToS ? "shalimar" : "c", source.name()).serialise());
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
