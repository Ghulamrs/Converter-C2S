#include "Converter.h"

#include "Source.h"
#include "c/CLexer.h"
#include "c/CMacro.h"
#include "c/CParser.h"
#include "c/CPreScan.h"
#include "c/CPrinter.h"
#include "convert/CToS.h"
#include "convert/SToC.h"
#include "s/SFrontEnd.h"
#include "s/SPrinter.h"

namespace c2s {

Converter::Result Converter::convert(const std::string &sourceText,
                                     const std::string &name,
                                     Direction direction,
                                     const Permissions &permissions,
                                     bool emitIncludes) {
    Result result;
    Diagnostics diagnostics;
    Source source(name, sourceText);

    if (direction == Direction::CToShalimar) {
        CPreScan prescan;
        if (!prescan.run(source, diagnostics)) {
            result.questions = prescan.pending();
            result.diagnostics = diagnostics.messages();
            result.summary = diagnostics.summary();
            return result;
        }

        CLexResult lexed = CLexer(prescan.text()).tokenize();
        if (lexed.failed) {
            diagnostics.report(Severity::SyntaxError, source,
                               source.locate(lexed.errorOffset), "C1001",
                               lexed.error);
            result.diagnostics = diagnostics.messages();
            result.summary = diagnostics.summary();
            return result;
        }

        // The #defines that name a value go in where they were written.
        // After lexing, so that the offsets a diagnostic quotes by still
        // point into the file the author edited - see CMacro.h.
        if (!expandMacros(prescan.macros(), lexed.tokens, source, diagnostics)) {
            result.diagnostics = diagnostics.messages();
            result.summary = diagnostics.summary();
            return result;
        }

        CParser parser(source, std::move(lexed.tokens), diagnostics);
        std::unique_ptr<CProgram> program = parser.parse();
        if (program == nullptr || diagnostics.hasErrors()) {
            result.diagnostics = diagnostics.messages();
            result.summary = diagnostics.summary();
            return result;
        }

        CToS converter(source, diagnostics, permissions);
        std::unique_ptr<shalimar::Program> converted = converter.convert(*program);

        SPrinter printer;
        result.output = printer.print(*converted);
        result.beyondCount = converter.beyondCount();
        result.ok = true;
        result.diagnostics = diagnostics.messages();
        result.summary = diagnostics.summary();
        return result;
    }

    if (direction == Direction::ShalimarToC) {
        SFrontEnd frontEnd;
        std::unique_ptr<shalimar::Program> program =
            frontEnd.parseAndCheck(source, diagnostics);
        if (program == nullptr || diagnostics.hasErrors()) {
            result.diagnostics = diagnostics.messages();
            result.summary = diagnostics.summary();
            return result;
        }

        SToC converter(source, diagnostics);
        std::unique_ptr<CProgram> converted = converter.convert(*program);

        std::string text;
        if (emitIncludes) {
            std::vector<std::string> includes = converter.includes();
            for (std::size_t i = 0; i < includes.size(); ++i) {
                text += "#include <" + includes[i] + ">\n";
            }
            if (!includes.empty()) text += "\n";
        }
        const std::string preamble = converter.preamble();
        if (!preamble.empty()) {
            text += preamble;
            text += "\n";
        }
        CPrinter printer;
        text += printer.print(*converted);

        result.output = text;
        result.beyondCount = converter.beyondCount();
        result.ok = true;
        result.diagnostics = diagnostics.messages();
        result.summary = diagnostics.summary();
        return result;
    }

    diagnostics.report(Severity::SyntaxError, "C0005",
                       "no direction to convert '" + name + "' in");
    result.diagnostics = diagnostics.messages();
    result.summary = diagnostics.summary();
    return result;
}

Converter::Result Converter::canonicalise(const std::string &sourceText,
                                          const std::string &name,
                                          Direction direction) {
    Result result;
    Diagnostics diagnostics;
    Source source(name, sourceText);

    if (direction == Direction::ShalimarToC) {
        // Shalimar in, canonical Shalimar out.
        SFrontEnd frontEnd;
        std::unique_ptr<shalimar::Program> program =
            frontEnd.parseAndCheck(source, diagnostics);
        if (program == nullptr || diagnostics.hasErrors()) {
            result.diagnostics = diagnostics.messages();
            result.summary = diagnostics.summary();
            return result;
        }
        SPrinter printer;
        result.output = printer.print(*program);
        result.ok = true;
    } else {
        // C in, canonical C out, with the recorded #include lines restored
        // so the result still compiles under cc1.
        CPreScan prescan;
        if (!prescan.run(source, diagnostics)) {
            result.questions = prescan.pending();
            result.diagnostics = diagnostics.messages();
            result.summary = diagnostics.summary();
            return result;
        }
        CLexResult lexed = CLexer(prescan.text()).tokenize();
        if (lexed.failed) {
            diagnostics.report(Severity::SyntaxError, source,
                               source.locate(lexed.errorOffset), "C1001",
                               lexed.error);
            result.diagnostics = diagnostics.messages();
            result.summary = diagnostics.summary();
            return result;
        }
        // The #defines that name a value go in where they were written.
        // After lexing, so that the offsets a diagnostic quotes by still
        // point into the file the author edited - see CMacro.h.
        if (!expandMacros(prescan.macros(), lexed.tokens, source, diagnostics)) {
            result.diagnostics = diagnostics.messages();
            result.summary = diagnostics.summary();
            return result;
        }

        CParser parser(source, std::move(lexed.tokens), diagnostics);
        std::unique_ptr<CProgram> program = parser.parse();
        if (program == nullptr || diagnostics.hasErrors()) {
            result.diagnostics = diagnostics.messages();
            result.summary = diagnostics.summary();
            return result;
        }
        std::string text;
        const std::vector<CPreScan::Include> &includes = prescan.includes();
        for (std::size_t i = 0; i < includes.size(); ++i) {
            text += includes[i].angled ? "#include <" + includes[i].header + ">\n"
                                       : "#include \"" + includes[i].header + "\"\n";
        }
        if (!text.empty()) text += "\n";
        CPrinter printer;
        text += printer.print(*program);
        result.output = text;
        result.ok = true;
    }

    result.diagnostics = diagnostics.messages();
    result.summary = diagnostics.summary();
    return result;
}

namespace {

bool endsWith(const std::string &text, const std::string &suffix) {
    if (text.size() < suffix.size()) return false;
    return text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

}  // namespace

Converter::Result Converter::convertFile(const std::string &sourceText,
                                         const std::string &name,
                                         const Permissions &permissions) {
    Direction direction = Direction::Infer;
    if (endsWith(name, ".c")) direction = Direction::CToShalimar;
    else if (endsWith(name, ".shm") || endsWith(name, ".shl")) {
        direction = Direction::ShalimarToC;
    }

    if (direction == Direction::Infer) {
        Result result;
        Diagnostics diagnostics;
        diagnostics.report(Severity::SyntaxError, "C0005",
                           "cannot tell which way to convert '" + name +
                           "' - name it .c, .shm or .shl");
        result.diagnostics = diagnostics.messages();
        result.summary = diagnostics.summary();
        return result;
    }
    return convert(sourceText, name, direction, permissions);
}

}  // namespace c2s
