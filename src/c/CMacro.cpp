#include "CMacro.h"

#include <cstdio>
#include <map>
#include <set>
#include <string>

#include "../Diagnostics.h"
#include "../Source.h"
#include "CLexer.h"

namespace c2s {

namespace {

// A ceiling on how much one file's expansion may produce. Nothing legal
// comes near it; it is here so that a case this expander has not thought of
// ends as a diagnostic rather than as a converter that never returns.
const std::size_t kTokenCeiling = 2000000;

struct Ready {
    const CPreScan::Macro *macro = nullptr;
    std::vector<CToken> body;
    std::map<std::string, std::size_t> paramIndex;
};

class Expander {
public:
    Expander(const std::map<std::string, Ready> &table, const Source &source,
             Diagnostics &diagnostics)
        : table_(table), source_(source), diagnostics_(diagnostics) {}

    bool run(const std::vector<CToken> &in, std::vector<CToken> &out) {
        std::set<std::string> active;
        return expand(in, 0, in.size(), active, out);
    }

private:
    // The span [begin, end) of 'in', with every macro in it substituted,
    // appended to 'out'.
    bool expand(const std::vector<CToken> &in, std::size_t begin,
                std::size_t end, std::set<std::string> &active,
                std::vector<CToken> &out) {
        std::size_t i = begin;
        while (i < end) {
            const CToken &token = in[i];
            if (token.kind != CTokenKind::Identifier ||
                active.count(token.text) != 0) {
                if (!emit(token, out)) return false;
                ++i;
                continue;
            }
            std::map<std::string, Ready>::const_iterator found =
                table_.find(token.text);
            if (found == table_.end()) {
                if (!emit(token, out)) return false;
                ++i;
                continue;
            }
            const Ready &ready = found->second;

            if (!ready.macro->functionLike) {
                if (!substitute(ready, std::vector<std::vector<CToken> >(),
                                token, active, out)) {
                    return false;
                }
                ++i;
                continue;
            }

            // Function-like, and only a '(' makes it a call. A bare mention
            // of the name is not one - C's rule, and it is what lets a macro
            // share a name with something used as a plain identifier.
            if (i + 1 >= end || !in[i + 1].is("(")) {
                if (!emit(token, out)) return false;
                ++i;
                continue;
            }

            std::vector<std::vector<CToken> > args;
            std::size_t after = 0;
            if (!gather(in, i + 1, end, token, &args, &after)) return false;
            if (!checkArity(ready, args, token)) return false;

            // Arguments are expanded before they are substituted, and with
            // this macro still available to them - it is only disabled
            // inside its own replacement.
            std::vector<std::vector<CToken> > expandedArgs;
            for (std::size_t k = 0; k < args.size(); ++k) {
                std::vector<CToken> one;
                if (!expand(args[k], 0, args[k].size(), active, one)) return false;
                expandedArgs.push_back(one);
            }
            if (!substitute(ready, expandedArgs, token, active, out)) return false;
            i = after;
        }
        return true;
    }

    // The replacement, with parameters filled in, expanded again with this
    // macro disabled, and appended to 'out'.
    bool substitute(const Ready &ready,
                    const std::vector<std::vector<CToken> > &args,
                    const CToken &use, std::set<std::string> &active,
                    std::vector<CToken> &out) {
        std::vector<CToken> filled;
        for (std::size_t k = 0; k < ready.body.size(); ++k) {
            const CToken &token = ready.body[k];
            std::map<std::string, std::size_t>::const_iterator param =
                token.kind == CTokenKind::Identifier
                    ? ready.paramIndex.find(token.text)
                    : ready.paramIndex.end();
            if (param != ready.paramIndex.end() && param->second < args.size()) {
                const std::vector<CToken> &argument = args[param->second];
                for (std::size_t a = 0; a < argument.size(); ++a) {
                    filled.push_back(argument[a]);
                }
                continue;
            }
            // A replacement token was never written where it now stands, so
            // it answers for the place the macro was used.
            CToken moved = token;
            moved.offset = use.offset;
            filled.push_back(moved);
        }

        active.insert(ready.macro->name);
        const bool ok = expand(filled, 0, filled.size(), active, out);
        active.erase(ready.macro->name);
        return ok;
    }

    // The argument list of a call, starting at the '(' in 'in[open]'.
    bool gather(const std::vector<CToken> &in, std::size_t open, std::size_t end,
                const CToken &use, std::vector<std::vector<CToken> > *args,
                std::size_t *after) {
        int depth = 0;
        std::vector<CToken> current;
        std::size_t i = open;
        for (; i < end; ++i) {
            const CToken &token = in[i];
            if (token.is("(")) {
                ++depth;
                if (depth == 1) continue;      // the opening one is not an argument
            } else if (token.is(")")) {
                --depth;
                if (depth == 0) {
                    // '()' with nothing between it is one empty argument, or
                    // none at all; checkArity settles which.
                    if (!current.empty() || !args->empty()) args->push_back(current);
                    *after = i + 1;
                    return true;
                }
            } else if (token.is(",") && depth == 1) {
                args->push_back(current);
                current.clear();
                continue;
            }
            current.push_back(token);
        }
        report(use, "C1010", "a macro call with no closing ')'",
               "close the argument list on the line it opens - this converter "
               "does not join lines across a continuation");
        return false;
    }

    bool checkArity(const Ready &ready, const std::vector<std::vector<CToken> > &args,
                    const CToken &use) {
        const std::size_t want = ready.macro->params.size();
        if (args.size() == want) return true;
        char counts[96];
        std::snprintf(counts, sizeof counts, "wants %d, given %d",
                      static_cast<int>(want), static_cast<int>(args.size()));
        report(use, "C1011",
               "the macro '" + ready.macro->name + "' " + std::string(counts),
               "give it the arguments its #define names");
        return false;
    }

    bool emit(const CToken &token, std::vector<CToken> &out) {
        if (out.size() >= kTokenCeiling) {
            report(token, "C1012", "macro expansion that does not settle",
                   "one of the macros in this file expands without end - "
                   "check for two that name each other");
            return false;
        }
        out.push_back(token);
        return true;
    }

    void report(const CToken &at, const char *code, const std::string &message,
                const std::string &hint) {
        diagnostics_.report(Severity::SyntaxError, source_,
                            source_.locate(at.offset), code, message, hint);
    }

    const std::map<std::string, Ready> &table_;
    const Source &source_;
    Diagnostics &diagnostics_;
};

}  // namespace

bool expandMacros(const std::vector<CPreScan::Macro> &macros,
                  std::vector<CToken> &tokens,
                  const Source &source, Diagnostics &diagnostics) {
    if (macros.empty()) return true;

    // Each replacement is lexed once, here, rather than on every use. A
    // replacement that will not lex is the author's to fix, and it is named
    // at the #define rather than at some later place it was used.
    std::map<std::string, Ready> table;
    for (std::size_t i = 0; i < macros.size(); ++i) {
        const CPreScan::Macro &macro = macros[i];
        CLexResult lexed = CLexer(macro.body).tokenize();
        if (lexed.failed) {
            diagnostics.report(Severity::SyntaxError, source,
                               source.locate(macro.offset), "C1013",
                               "the replacement of '" + macro.name +
                               "' does not lex: " + lexed.error,
                               "correct the #define, or expand it by hand");
            return false;
        }
        Ready ready;
        ready.macro = &macro;
        ready.body = lexed.tokens;
        for (std::size_t k = 0; k < macro.params.size(); ++k) {
            ready.paramIndex[macro.params[k]] = k;
        }
        // A name defined twice: the last #define is what a C compiler would
        // have been using by the end of the file, and a converter that
        // silently used the first would differ from cc1 on the same text.
        table[macro.name] = ready;
    }

    std::vector<CToken> expanded;
    expanded.reserve(tokens.size());
    Expander expander(table, source, diagnostics);
    if (!expander.run(tokens, expanded)) return false;

    tokens.swap(expanded);
    return true;
}

}  // namespace c2s
