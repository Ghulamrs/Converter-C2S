#include "CPreScan.h"

#include <cctype>
#include <set>

#include "../Diagnostics.h"
#include "../Source.h"

namespace c2s {

namespace {

std::string directiveName(const std::string &line, std::size_t hash) {
    std::size_t i = hash + 1;
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    std::size_t begin = i;
    while (i < line.size() && std::isalpha(static_cast<unsigned char>(line[i])) != 0) ++i;
    return line.substr(begin, i - begin);
}

std::size_t afterDirective(const std::string &line, std::size_t hash) {
    std::size_t i = hash + 1;
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    while (i < line.size() && std::isalpha(static_cast<unsigned char>(line[i])) != 0) ++i;
    return i;
}

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

bool isNameChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

bool isNameStart(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) != 0 || c == '_';
}

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

bool parseDefine(const std::string &line, std::size_t afterName,
                 CPreScan::Macro *out) {
    std::size_t i = afterName;
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    if (i >= line.size() || !isNameStart(line[i])) return false;

    const std::size_t nameBegin = i;
    while (i < line.size() && isNameChar(line[i])) ++i;
    out->name = line.substr(nameBegin, i - nameBegin);

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

    if (out->body.find('#') != std::string::npos) return false;

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

std::string asWritten(const std::string &line, std::size_t hash) {
    std::size_t end = line.size();
    while (end > hash && (line[end - 1] == ' ' || line[end - 1] == '\t' ||
                          line[end - 1] == '\r')) --end;
    return line.substr(hash, end - hash);
}

}

bool CPreScan::run(const Source &source, Diagnostics &diagnostics) {
    text_ = source.text();
    includes_.clear();
    macros_.clear();
    pending_.clear();

    bool clean = true;
    const int lineTotal = source.lineCount();

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

        const std::size_t begin = source.offsetOfLine(lineNo);
        for (std::size_t k = 0; k < line.size(); ++k) {
            text_[begin + k] = ' ';
        }
    }

    return clean;
}

}
