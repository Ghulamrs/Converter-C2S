// --keep-check: A -> B -> A in memory, unedited and under five edits of B, each reported
// on one line; a test hook for tests/run.sh and run.ps1, not a conversion.
#include "KeepCheck.h"

#include <cctype>
#include <iostream>

#include "Converter.h"
#include "Keep.h"

namespace c2s {

namespace {

bool wordAt(const std::string &t, std::size_t i, const std::string &w) {
    if (t.compare(i, w.size(), w) != 0) return false;
    auto ic = [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; };
    if (i > 0 && ic(t[i - 1])) return false;
    return i + w.size() >= t.size() || !ic(t[i + w.size()]);
}

// Every offset of code in `t` - strings, characters and comments left out.
std::vector<bool> codeMask(const std::string &t) {
    std::vector<bool> m(t.size(), false);
    std::size_t i = 0;
    while (i < t.size()) {
        if (t.compare(i, 2, "//") == 0) { while (i < t.size() && t[i] != '\n') ++i; continue; }
        if (t.compare(i, 2, "/*") == 0) {
            std::size_t e = t.find("*/", i + 2);
            i = e == std::string::npos ? t.size() : e + 2;
            continue;
        }
        if (t[i] == '"' || t[i] == '\'') {
            char q = t[i++];
            while (i < t.size() && t[i] != q && t[i] != '\n') { if (t[i] == '\\') ++i; ++i; }
            ++i;
            continue;
        }
        m[i++] = true;
    }
    return m;
}

bool isFunction(const Chunk &c) {
    return c.kind == Chunk::Code && !c.key.empty() && c.text.find('{') != std::string::npos &&
           c.key.compare(0, 5, "uses:") != 0;
}

}

int KeepCheck::run(const std::string &a, const std::string &name, Direction dir,
                   const Permissions &permissions) {
    const bool toS = dir == Direction::CToShalimar;
    const Direction back = toS ? Direction::ShalimarToC : Direction::CToShalimar;
    Converter::Result forward = Converter::convert(a, name, dir, permissions);
    if (!forward.ok) { std::cout << "keep forward refused\n"; return 1; }
    const std::string b = forward.output;
    const Sidecar side = Sidecar::build(a, b, toS ? "c" : "shalimar", toS ? "shalimar" : "c", name);
    Sidecar reread;
    if (!Sidecar::parse(side.serialise(), reread)) { std::cout << "keep FAIL sidecar\n"; return 1; }
    const std::string backName = toS ? "edited.shm" : "edited.c";

    int failed = 0;
    auto attempt = [&](const std::string &what, const std::string &edited,
                       const std::string &excludedKey, const std::string &mustHave) {
        auto fresh = [&](std::string &out) {
            Converter::Result r = Converter::convert(edited, backName, back, permissions);
            out = r.output;
            return r.ok;
        };
        Keeper::Restored r = Keeper::restore(reread, edited, fresh);
        bool ok = r.usable;
        std::string why = r.note;
        if (ok && excludedKey == "\x01") {
            ok = r.text == a;
            if (!ok) why = "not byte-identical";
        } else if (ok) {
            std::vector<Chunk> ac = Segmenter::split(a);
            std::size_t at = 0;
            for (std::size_t i = 0; i < ac.size() && ok; ++i) {
                if (!excludedKey.empty() && ac[i].key == excludedKey) {
                    if (what == "delete-function" && r.text.find(ac[i].text) != std::string::npos) {
                        ok = false;
                        why = "the deleted construct is still there";
                    }
                    continue;
                }
                std::size_t f = r.text.find(ac[i].text, at);
                if (f == std::string::npos) {
                    ok = false;
                    why = "lost or changed: " + ac[i].key;
                } else {
                    at = f + ac[i].text.size();
                }
            }
            if (ok && !mustHave.empty() && r.text.find(mustHave) == std::string::npos) {
                ok = false;
                why = "the edit did not arrive";
            }
        }
        std::cout << "keep " << what << (ok ? " ok" : " FAIL") << (ok ? "" : ": " + why) << '\n';
        if (!ok) ++failed;
    };
    auto skip = [&](const std::string &what) { std::cout << "keep " << what << " skip\n"; };

    attempt("identity", b, "\x01", "");

    std::vector<Chunk> bc = Segmenter::split(b);
    long target = -1;
    for (std::size_t i = 0; i < bc.size(); ++i) {
        if (isFunction(bc[i]) && (target < 0 || bc[i].key == "main")) target = static_cast<long>(i);
    }
    auto rebuilt = [&](std::size_t at, const std::string &text) {
        std::string o;
        for (std::size_t i = 0; i < bc.size(); ++i) o += i == at ? text : bc[i].text;
        return o;
    };

    if (target < 0) {
        skip("constant"); skip("add-statement"); skip("rename-local");
    } else {
        const std::size_t ti = static_cast<std::size_t>(target);
        const std::string body = bc[ti].text, key = bc[ti].key;
        const std::vector<bool> code = codeMask(body);
        const std::size_t open = body.find('{');

        std::size_t d = open;
        while (d < body.size() && !(code[d] && std::isdigit(static_cast<unsigned char>(body[d])) &&
                                    !(std::isalnum(static_cast<unsigned char>(body[d - 1])) || body[d - 1] == '_')))
            ++d;
        if (d < body.size()) {
            attempt("constant", rebuilt(ti, body.substr(0, d) + "1" + body.substr(d)), key, "");
        } else {
            skip("constant");
        }

        const std::size_t nl = body.find('\n', open);
        const std::string stmt = toS ? "  keepadd : 1\n" : "    int keepadd = 1;\n";
        attempt("add-statement", rebuilt(ti, body.substr(0, nl + 1) + stmt + body.substr(nl + 1)),
                key, "keepadd");

        std::string local;
        std::vector<std::string> ids = Segmenter::identifiers(body.substr(open));
        for (std::size_t i = 0; i + 1 < ids.size() && local.empty(); ++i) {
            if (ids[i] == "int" || ids[i] == "double" || ids[i] == "real" || ids[i] == "char")
                local = ids[i + 1];
        }
        if (toS && local.empty()) {
            for (std::size_t p = open; p < body.size() && local.empty(); p = body.find('\n', p) + 1) {
                std::size_t q = body.find_first_not_of(" \t", p);
                std::size_t e = q;
                while (e < body.size() && (std::isalnum(static_cast<unsigned char>(body[e])) || body[e] == '_')) ++e;
                if (e > q && body.compare(e, 3, " : ") == 0) local = body.substr(q, e - q);
                if (body.find('\n', p) == std::string::npos) break;
            }
        }
        if (local.empty() || local == "main") {
            skip("rename-local");
        } else {
            std::string out;
            for (std::size_t i = 0; i < body.size(); ++i) {
                if (code[i] && wordAt(body, i, local)) { out += local + "_kr"; i += local.size() - 1; }
                else out += body[i];
            }
            attempt("rename-local", rebuilt(ti, out), key, local + "_kr");
        }
    }

    long victim = -1;
    for (std::size_t i = 0; i < bc.size() && victim < 0; ++i) {
        if (!isFunction(bc[i]) || bc[i].key == "main") continue;
        bool called = false;
        for (std::size_t k = 0; k < bc.size() && !called; ++k) {
            if (k == i) continue;
            std::vector<std::string> ids = Segmenter::identifiers(bc[k].text);
            for (std::size_t q = 0; q < ids.size(); ++q) if (ids[q] == bc[i].key) called = true;
        }
        if (!called) victim = static_cast<long>(i);
    }
    if (victim < 0) skip("delete-function");
    else attempt("delete-function", rebuilt(static_cast<std::size_t>(victim), ""), bc[static_cast<std::size_t>(victim)].key, "");

    const std::string added = toS ? "\nfun <int> = keepadded(x: int) {\n  return x + 1\n}\n"
                                  : "\nint keepadded(int x)\n{\n    return x + 1;\n}\n";
    attempt("add-function", b + added, "", "keepadded");

    return failed == 0 ? 0 : 1;
}

}
