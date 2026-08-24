#include "CPreScan.h"

#include <cctype>

#include "../Diagnostics.h"
#include "../Source.h"

namespace c2s {

namespace {

// The directive name after '#', with any space between them skipped -
// '#  define' is a directive as surely as '#define'.
std::string directiveName(const std::string &line, std::size_t hash) {
    std::size_t i = hash + 1;
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    std::size_t begin = i;
    while (i < line.size() && std::isalpha(static_cast<unsigned char>(line[i])) != 0) ++i;
    return line.substr(begin, i - begin);
}

// What a reader should do about each directive, said once each way.
std::string adviceFor(const std::string &name) {
    if (name == "define")
        return "expand or inline the macro by hand, or make it a variable or a function";
    if (name == "undef")
        return "remove it along with the #define it cancels";
    if (name == "if" || name == "ifdef" || name == "ifndef" ||
        name == "elif" || name == "else" || name == "endif")
        return "decide the condition by hand and keep only the branch that is wanted";
    if (name == "pragma")
        return "remove it; a pragma has no meaning to the target language";
    if (name == "error")
        return "resolve whatever the #error guards and remove it";
    if (name == "line")
        return "remove it; the line numbers of the file itself are what diagnostics use";
    return "resolve it by hand and remove it";
}

bool parseInclude(const std::string &line, std::size_t afterName,
                  CPreScan::Include *out) {
    std::size_t i = afterName;
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    if (i >= line.size()) return false;

    char open = line[i];
    char close = open == '<' ? '>' : (open == '"' ? '"' : '\0');
    if (close == '\0') return false;

    std::size_t begin = ++i;
    while (i < line.size() && line[i] != close) ++i;
    if (i >= line.size()) return false;

    out->header = line.substr(begin, i - begin);
    out->angled = open == '<';
    return true;
}

}  // namespace

bool CPreScan::run(const Source &source, Diagnostics &diagnostics) {
    text_ = source.text();
    includes_.clear();

    bool clean = true;

    // Line by line over the original text. A '#' that is not the first
    // non-blank character of its line is not a directive in any program cc1
    // would accept, and '#' appears nowhere else in C89 outside a literal -
    // which a first-column scan never reads into.
    const int lines = source.lineCount();
    for (int lineNo = 1; lineNo <= lines; ++lineNo) {
        const std::string line = source.line(lineNo);

        std::size_t i = 0;
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
        if (i >= line.size() || line[i] != '#') continue;

        const std::string name = directiveName(line, i);
        const Location where(source.name(), lineNo, static_cast<int>(i) + 1);

        if (name == "include") {
            Include include;
            include.line = lineNo;
            if (parseInclude(line, i + 1 + name.size(), &include)) {
                includes_.push_back(include);
            }
            // Dropped either way: a malformed include is a syntax problem
            // cc1 will name better than a converter can.
        } else if (name.empty()) {
            diagnostics.report(Severity::PreprocessorFix, source, where, "P0100",
                               "a '#' line the converter does not read",
                               "remove it before converting");
            clean = false;
        } else {
            diagnostics.report(Severity::PreprocessorFix, source, where, "P0101",
                               "#" + name + " must be resolved before conversion",
                               adviceFor(name));
            clean = false;
        }

        // Blank the directive out of the text handed to the lexer, keeping
        // every offset and line number pointing where it did. A directive
        // may not continue over a backslash newline here; cc1 accepts that,
        // and a program using it will be refused by this scan's parse later,
        // which is the honest outcome until continuation is taught.
        const std::size_t begin = source.offsetOfLine(lineNo);
        for (std::size_t k = 0; k < line.size(); ++k) {
            text_[begin + k] = ' ';
        }
    }

    return clean;
}

}  // namespace c2s
