#include "CPreScan.h"

#include <cctype>
#include <set>

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

// Just past the directive's name, wherever the spaces fell: '#  define X 1'
// has to give the same answer as '#define X 1'.
std::size_t afterDirective(const std::string &line, std::size_t hash) {
    std::size_t i = hash + 1;
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    while (i < line.size() && std::isalpha(static_cast<unsigned char>(line[i])) != 0) ++i;
    return i;
}

// What a reader should do about each directive, said once each way.
std::string adviceFor(const std::string &name) {
    if (name == "define")
        return "expand or inline the macro by hand, or make it a variable or a function";
    // The two #define cases that get here have their own advice at the call
    // site; this line is the fallback for one that reaches neither.
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

bool isNameChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

bool isNameStart(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) != 0 || c == '_';
}

// Every identifier on the rest of a directive line. Used on the conditional
// directives to learn which names decide something - over-collecting is
// safe here, because the only cost is asking about a #define that might not
// have needed asking about.
void namesIn(const std::string &line, std::size_t from,
             std::set<std::string> &into) {
    std::size_t i = from;
    while (i < line.size()) {
        if (!isNameStart(line[i])) { ++i; continue; }
        const std::size_t begin = i;
        while (i < line.size() && isNameChar(line[i])) ++i;
        into.insert(line.substr(begin, i - begin));
    }
}

// '#define NAME body' or '#define NAME(a, b) body', already known to start
// with 'define'. Returns false when the line is not a shape this can read,
// which sends it back to the ask-first path rather than guessing.
bool parseDefine(const std::string &line, std::size_t afterName,
                 CPreScan::Macro *out) {
    std::size_t i = afterName;
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    if (i >= line.size() || !isNameStart(line[i])) return false;

    const std::size_t nameBegin = i;
    while (i < line.size() && isNameChar(line[i])) ++i;
    out->name = line.substr(nameBegin, i - nameBegin);

    // A '(' with no space before it makes it function-like. With a space it
    // is the start of the replacement, which is C's rule and a real
    // distinction: '#define A (x)' is not a macro of one parameter.
    if (i < line.size() && line[i] == '(') {
        out->functionLike = true;
        ++i;
        for (;;) {
            while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
            if (i < line.size() && line[i] == ')') { ++i; break; }
            if (i >= line.size() || !isNameStart(line[i])) return false;
            const std::size_t paramBegin = i;
            while (i < line.size() && isNameChar(line[i])) ++i;
            out->params.push_back(line.substr(paramBegin, i - paramBegin));
            while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
            if (i < line.size() && line[i] == ',') { ++i; continue; }
            if (i < line.size() && line[i] == ')') { ++i; break; }
            return false;
        }
    }

    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    std::size_t end = line.size();
    while (end > i && (line[end - 1] == ' ' || line[end - 1] == '\t' ||
                       line[end - 1] == '\r')) --end;
    out->body = line.substr(i, end - i);

    // '#' and '##' in a replacement are stringify and paste. Neither has a
    // token-level equivalent worth guessing at, and both change what the
    // text means rather than what it is, so they go back to the author.
    if (out->body.find('#') != std::string::npos) return false;
    // A continuation carries the replacement onto the next line, which this
    // scan does not read - see the note at the blanking below.
    if (!out->body.empty() && out->body[out->body.size() - 1] == '\\') return false;
    return true;
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

// The directive as the author wrote it: from the '#' to the last thing on
// the line. The leading indent goes, because the list is read as a list.
std::string asWritten(const std::string &line, std::size_t hash) {
    std::size_t end = line.size();
    while (end > hash && (line[end - 1] == ' ' || line[end - 1] == '\t' ||
                          line[end - 1] == '\r')) --end;
    return line.substr(hash, end - hash);
}

}  // namespace

bool CPreScan::run(const Source &source, Diagnostics &diagnostics) {
    text_ = source.text();
    includes_.clear();
    macros_.clear();
    pending_.clear();

    bool clean = true;
    const int lineTotal = source.lineCount();

    // First pass: which names does a conditional turn on? Only a #define one
    // of those tests is a question for the author - the rest are values, and
    // values get substituted. The whole file has to be read before any
    // #define can be judged, because a #define may stand after the #ifdef
    // that tests it, and often does.
    //
    // Collecting every identifier on the line rather than parsing the
    // expression over-collects - 'defined' and any name in an arithmetic
    // #if come too - and that is the safe direction to be wrong in: the
    // cost is asking about a #define that might not have needed asking.
    std::set<std::string> decidedBy;
    for (int lineNo = 1; lineNo <= lineTotal; ++lineNo) {
        const std::string line = source.line(lineNo);
        std::size_t i = 0;
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
        if (i >= line.size() || line[i] != '#') continue;
        const std::string name = directiveName(line, i);
        if (name == "if" || name == "ifdef" || name == "ifndef" || name == "elif") {
            namesIn(line, i + 1, decidedBy);
        }
    }

    // Line by line over the original text. A '#' that is not the first
    // non-blank character of its line is not a directive in any program cc1
    // would accept, and '#' appears nowhere else in C89 outside a literal -
    // which a first-column scan never reads into.
    for (int lineNo = 1; lineNo <= lineTotal; ++lineNo) {
        const std::string line = source.line(lineNo);

        std::size_t i = 0;
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
        if (i >= line.size() || line[i] != '#') continue;

        const std::string name = directiveName(line, i);
        const Location where(source.name(), lineNo, static_cast<int>(i) + 1);

        if (name == "include") {
            Include include;
            include.line = lineNo;
            if (parseInclude(line, afterDirective(line, i), &include)) {
                includes_.push_back(include);
            }
            // Dropped either way: a malformed include is a syntax problem
            // cc1 will name better than a converter can.
        } else if (name == "define") {
            Macro macro;
            macro.line = lineNo;
            macro.offset = source.offsetOfLine(lineNo) + i;
            if (!parseDefine(line, afterDirective(line, i), &macro)) {
                diagnostics.report(Severity::PreprocessorFix, source, where, "P0102",
                                   "#define in a form this converter cannot expand",
                                   "expand it by hand - a '#' or '##' in the "
                                   "replacement, or a line continuation, has no "
                                   "token-level equivalent here");
                pending_.push_back(asWritten(line, i));
                clean = false;
            } else if (decidedBy.count(macro.name) != 0) {
                diagnostics.report(Severity::PreprocessorFix, source, where, "P0103",
                                   "#define " + macro.name +
                                   " decides which program this is - a "
                                   "conditional tests it",
                                   "settle the condition by hand and keep only "
                                   "the branch that is wanted, then remove both "
                                   "the #define and the #if that reads it");
                pending_.push_back(asWritten(line, i));
                clean = false;
            } else {
                // A value, not a decision. Substituted after lexing.
                macros_.push_back(macro);
            }
        } else if (name.empty()) {
            diagnostics.report(Severity::PreprocessorFix, source, where, "P0100",
                               "a '#' line the converter does not read",
                               "remove it before converting");
            pending_.push_back(asWritten(line, i));
            clean = false;
        } else {
            diagnostics.report(Severity::PreprocessorFix, source, where, "P0101",
                               "#" + name + " must be resolved before conversion",
                               adviceFor(name));
            pending_.push_back(asWritten(line, i));
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
