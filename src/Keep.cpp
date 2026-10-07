// Keep: the segmenter, the sidecar and the restore (see Keep.h and docs/KEEP.md).
#include "Keep.h"

#include <cctype>
#include <cstdlib>
#include <set>
#include <sstream>

namespace c2s {

namespace {

const char *kMagic = "c2s-keep 1";
const char *kBeyond = "#BEYOND SHALIMAR";

bool identStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool identChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

std::size_t lineEnd(const std::string &t, std::size_t p) {
    std::size_t e = t.find('\n', p);
    return e == std::string::npos ? t.size() : e + 1;
}

std::string trimmed(const std::string &s) {
    std::size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    std::size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

bool startsWith(const std::string &s, std::size_t at, const std::string &p) {
    return s.compare(at, p.size(), p) == 0;
}

std::size_t firstNonSpace(const std::string &t, std::size_t p, std::size_t e) {
    while (p < e && (t[p] == ' ' || t[p] == '\t' || t[p] == '\r')) ++p;
    return p;
}

// The tokens of a body that matter for its key: identifiers and single punctuation,
// with strings, characters and comments stepped over.
std::vector<std::string> tokens(const std::string &t) {
    std::vector<std::string> out;
    std::size_t i = 0;
    while (i < t.size()) {
        char c = t[i];
        if (startsWith(t, i, "//")) { i = lineEnd(t, i); continue; }
        if (startsWith(t, i, "/*")) {
            std::size_t e = t.find("*/", i + 2);
            i = e == std::string::npos ? t.size() : e + 2;
            continue;
        }
        if (c == '"' || c == '\'') {
            ++i;
            while (i < t.size() && t[i] != c && t[i] != '\n') { if (t[i] == '\\') ++i; ++i; }
            ++i;
            out.push_back("\"");
            continue;
        }
        if (identStart(c)) {
            std::size_t b = i;
            while (i < t.size() && identChar(t[i])) ++i;
            out.push_back(t.substr(b, i - b));
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(c)) || std::isdigit(static_cast<unsigned char>(c))) {
            ++i;
            continue;
        }
        out.push_back(std::string(1, c));
        ++i;
    }
    return out;
}

// What a construct declares: the function, variable or type it names.
std::string keyOf(const Chunk &chunk, std::size_t bodyAt) {
    const std::string body = chunk.text.substr(bodyAt);
    if (chunk.kind == Chunk::Directive) {
        std::string k;
        bool space = false;
        for (std::size_t i = 0; i < body.size(); ++i) {
            char c = body[i];
            if (std::isspace(static_cast<unsigned char>(c))) { space = !k.empty(); continue; }
            if (space) k += ' ';
            space = false;
            k += c;
        }
        return "#" + k;
    }
    if (chunk.kind == Chunk::Beyond) return "beyond:" + trimmed(body);
    if (chunk.kind == Chunk::Trailer) return "";
    std::vector<std::string> tk = tokens(body);
    if (tk.empty()) return "";
    if (tk[0] == "uses") return "uses:" + trimmed(body);
    if (tk[0] == "fun") {
        for (std::size_t i = 1; i < tk.size(); ++i) {
            if (tk[i] == "(" && identStart(tk[i - 1][0])) return tk[i - 1];
        }
        return "";
    }
    std::string last;
    int depth = 0;
    for (std::size_t i = 0; i < tk.size(); ++i) {
        const std::string &s = tk[i];
        if (identStart(s[0])) { if (depth == 0) last = s; continue; }
        if (depth == 0 && (s == "(" || s == "=" || s == ":" || s == "[" || s == ";" ||
                           s == "," || s == "{")) {
            return last;
        }
        if (s == "(" || s == "[" || s == "{") ++depth;
        if (s == ")" || s == "]" || s == "}") --depth;
    }
    return last;
}

// The last significant character of [b, e), comments and strings left out, and the
// brace and parenthesis depth across it, carried from the line before.
char scanLine(const std::string &t, std::size_t b, std::size_t e, int &depth, bool &inBlock) {
    char last = 0;
    std::size_t i = b;
    while (i < e) {
        if (inBlock) {
            if (startsWith(t, i, "*/")) { inBlock = false; i += 2; } else ++i;
            continue;
        }
        char c = t[i];
        if (startsWith(t, i, "//")) break;
        if (startsWith(t, i, "/*")) { inBlock = true; i += 2; continue; }
        if (c == '"' || c == '\'') {
            ++i;
            while (i < e && t[i] != c && t[i] != '\n') { if (t[i] == '\\') ++i; ++i; }
            ++i;
            last = c;
            continue;
        }
        if (c == '(' || c == '[' || c == '{') ++depth;
        if (c == ')' || c == ']' || c == '}') --depth;
        if (!std::isspace(static_cast<unsigned char>(c))) last = c;
        ++i;
    }
    return last;
}

bool blankLine(const std::string &t, std::size_t b, std::size_t e) {
    return firstNonSpace(t, b, e) >= e || t[firstNonSpace(t, b, e)] == '\n';
}

}

std::vector<Chunk> Segmenter::split(const std::string &t) {
    std::vector<Chunk> out;
    std::size_t p = 0;
    while (p < t.size()) {
        const std::size_t start = p;
        Chunk chunk;
        std::size_t bodyAt = std::string::npos;
        // Leading trivia: blank lines and comment-only lines; a BEYOND block stops it.
        while (p < t.size()) {
            std::size_t e = lineEnd(t, p);
            if (blankLine(t, p, e)) { p = e; continue; }
            std::size_t f = firstNonSpace(t, p, e);
            bool line = startsWith(t, f, "//"), block = startsWith(t, f, "/*");
            if (!line && !block) break;
            std::size_t after = f + 2;
            while (after < e && t[after] == ' ') ++after;
            if (startsWith(t, after, kBeyond)) {
                if (p > start) break;
                chunk.kind = Chunk::Beyond;
                bodyAt = 0;
                if (line) {
                    p = e;
                    while (p < t.size()) {
                        std::size_t e2 = lineEnd(t, p), f2 = firstNonSpace(t, p, e2);
                        if (!startsWith(t, f2, "//")) break;
                        std::size_t a2 = f2 + 2;
                        while (a2 < e2 && t[a2] == ' ') ++a2;
                        if (startsWith(t, a2, kBeyond)) break;
                        p = e2;
                    }
                } else {
                    std::size_t c = t.find("*/", f + 2);
                    p = c == std::string::npos ? t.size() : lineEnd(t, c);
                }
                break;
            }
            if (line) { p = e; continue; }
            std::size_t c = t.find("*/", f + 2);
            if (c == std::string::npos) { p = t.size(); break; }
            std::size_t e2 = lineEnd(t, c);
            if (!blankLine(t, c + 2, e2)) break;
            p = e2;
        }
        if (chunk.kind != Chunk::Beyond) {
            if (p >= t.size()) {
                chunk.kind = Chunk::Trailer;
                bodyAt = 0;
            } else {
                bodyAt = p - start;
                std::size_t f = firstNonSpace(t, p, lineEnd(t, p));
                if (t[f] == '#') {
                    chunk.kind = Chunk::Directive;
                    std::size_t e = lineEnd(t, p);
                    while (e < t.size() && e >= 2 && trimmed(t.substr(p, e - p)).size() &&
                           trimmed(t.substr(p, e - p)).back() == '\\') {
                        p = e;
                        e = lineEnd(t, p);
                    }
                    p = e;
                } else {
                    int depth = 0;
                    bool inBlock = false;
                    while (p < t.size()) {
                        std::size_t e = lineEnd(t, p);
                        char last = scanLine(t, p, e, depth, inBlock);
                        p = e;
                        if (depth > 0 || inBlock) continue;
                        if (last && std::string(",=+-*/&|(<>!:").find(last) != std::string::npos) continue;
                        std::size_t q = p;
                        while (q < t.size() && blankLine(t, q, lineEnd(t, q))) q = lineEnd(t, q);
                        if (q < t.size() && t[firstNonSpace(t, q, lineEnd(t, q))] == '{') continue;
                        break;
                    }
                }
            }
        }
        chunk.text = t.substr(start, p - start);
        chunk.key = keyOf(chunk, bodyAt == std::string::npos ? 0 : bodyAt);
        out.push_back(chunk);
    }
    return out;
}

std::vector<std::string> Segmenter::identifiers(const std::string &text) {
    std::vector<std::string> out;
    std::vector<std::string> tk = tokens(text);
    for (std::size_t i = 0; i < tk.size(); ++i) if (identStart(tk[i][0])) out.push_back(tk[i]);
    return out;
}

bool Segmenter::beyondLines(const std::string &block, std::vector<std::string> &lines) {
    lines.clear();
    std::size_t p = 0;
    bool header = false, slashes = false;
    while (p < block.size()) {
        std::size_t e = lineEnd(block, p);
        std::string raw = block.substr(p, e - p);
        while (!raw.empty() && (raw.back() == '\n' || raw.back() == '\r')) raw.pop_back();
        p = e;
        std::size_t f = firstNonSpace(raw, 0, raw.size());
        if (!header) {
            if (f >= raw.size()) continue;
            slashes = startsWith(raw, f, "//");
            if (!slashes && !startsWith(raw, f, "/*")) return false;
            std::size_t a = f + 2;
            while (a < raw.size() && raw[a] == ' ') ++a;
            if (!startsWith(raw, a, kBeyond)) return false;
            header = true;
            continue;
        }
        if (slashes) {
            if (!startsWith(raw, f, "//    ")) return false;
            lines.push_back(raw.substr(f + 6));
        } else {
            if (startsWith(raw, f, "*/")) return true;
            if (!startsWith(raw, f, "   ")) return false;
            lines.push_back(raw.substr(f + 3));
        }
    }
    return header && slashes;
}

namespace {

// Longest common subsequence of two sequences under `same`, as matched index pairs.
template <class A, class B, class Same>
std::vector<std::pair<std::size_t, std::size_t>> lcs(const A &a, const B &b, Same same) {
    const std::size_t n = a.size(), m = b.size();
    std::vector<std::vector<int>> d(n + 1, std::vector<int>(m + 1, 0));
    for (std::size_t i = n; i-- > 0;)
        for (std::size_t j = m; j-- > 0;)
            d[i][j] = same(a[i], b[j]) ? d[i + 1][j + 1] + 1
                                       : (d[i + 1][j] > d[i][j + 1] ? d[i + 1][j] : d[i][j + 1]);
    std::vector<std::pair<std::size_t, std::size_t>> out;
    std::size_t i = 0, j = 0;
    while (i < n && j < m) {
        if (same(a[i], b[j])) { out.push_back(std::make_pair(i, j)); ++i; ++j; }
        else if (d[i + 1][j] >= d[i][j + 1]) ++i;
        else ++j;
    }
    return out;
}

}

Sidecar Sidecar::build(const std::string &source, const std::string &output,
                       const std::string &from, const std::string &to,
                       const std::string &sourceName) {
    Sidecar s;
    s.from = from;
    s.to = to;
    s.sourceName = sourceName;
    std::vector<Chunk> a = Segmenter::split(source), b = Segmenter::split(output);
    auto pairs = lcs(a, b, [](const Chunk &x, const Chunk &y) {
        return !x.key.empty() && x.key == y.key;
    });
    std::size_t i = 0, j = 0;
    for (std::size_t k = 0; k <= pairs.size(); ++k) {
        std::size_t pi = k < pairs.size() ? pairs[k].first : a.size();
        std::size_t pj = k < pairs.size() ? pairs[k].second : b.size();
        for (; i < pi; ++i) s.segments.push_back(Segment{a[i].text, "", a[i].key});
        for (; j < pj; ++j) s.segments.push_back(Segment{"", b[j].text, b[j].key});
        if (k < pairs.size()) {
            s.segments.push_back(Segment{a[i].text, b[j].text, a[i].key});
            ++i;
            ++j;
        }
    }
    return s;
}

std::string Sidecar::serialise() const {
    std::ostringstream o;
    o << kMagic << "\nfrom " << from << "\nto " << to << "\nsource " << sourceName
      << "\nsegments " << segments.size() << "\n";
    for (std::size_t i = 0; i < segments.size(); ++i) {
        const Segment &g = segments[i];
        std::string key = g.key;
        for (std::size_t c = 0; c < key.size(); ++c) if (key[c] == '\n' || key[c] == '\r') key[c] = ' ';
        o << "seg " << g.source.size() << ' ' << g.output.size() << ' ' << key << '\n'
          << g.source << g.output << '\n';
    }
    return o.str();
}

bool Sidecar::parse(const std::string &text, Sidecar &s) {
    std::size_t p = 0;
    auto line = [&](std::string &l) {
        if (p >= text.size()) return false;
        std::size_t e = text.find('\n', p);
        if (e == std::string::npos) return false;
        l = text.substr(p, e - p);
        if (!l.empty() && l.back() == '\r') l.pop_back();
        p = e + 1;
        return true;
    };
    std::string l;
    if (!line(l) || l != kMagic) return false;
    if (!line(l) || l.compare(0, 5, "from ") != 0) return false;
    s.from = l.substr(5);
    if (!line(l) || l.compare(0, 3, "to ") != 0) return false;
    s.to = l.substr(3);
    if (!line(l) || l.compare(0, 7, "source ") != 0) return false;
    s.sourceName = l.substr(7);
    if (!line(l) || l.compare(0, 9, "segments ") != 0) return false;
    const long count = std::strtol(l.c_str() + 9, nullptr, 10);
    for (long k = 0; k < count; ++k) {
        if (!line(l) || l.compare(0, 4, "seg ") != 0) return false;
        char *end = nullptr;
        const unsigned long ns = std::strtoul(l.c_str() + 4, &end, 10);
        const unsigned long no = std::strtoul(end, &end, 10);
        Segment g;
        g.key = *end == ' ' ? std::string(end + 1) : std::string();
        if (p + ns + no + 1 > text.size()) return false;
        g.source = text.substr(p, ns);
        g.output = text.substr(p + ns, no);
        p += ns + no;
        if (text[p] != '\n') return false;
        ++p;
        s.segments.push_back(g);
    }
    return true;
}

std::string Sidecar::output() const {
    std::string o;
    for (std::size_t i = 0; i < segments.size(); ++i) o += segments[i].output;
    return o;
}

std::string Sidecar::source() const {
    std::string o;
    for (std::size_t i = 0; i < segments.size(); ++i) o += segments[i].source;
    return o;
}

namespace {

std::string decodeBeyond(const Chunk &c) {
    std::vector<std::string> lines;
    if (!Segmenter::beyondLines(c.text, lines)) return "";
    std::string o;
    for (std::size_t i = 0; i < lines.size(); ++i) o += lines[i] + "\n";
    return o;
}

}

Keeper::Restored Keeper::restore(const Sidecar &sidecar, const std::string &edited,
                                 const std::function<bool(std::string &)> &convertFresh) {
    Restored r;
    if (edited == sidecar.output()) {
        r.usable = true;
        r.text = sidecar.source();
        r.kept = static_cast<int>(sidecar.segments.size());
        return r;
    }
    std::vector<Chunk> b = Segmenter::split(edited);
    std::vector<std::size_t> withOut;
    for (std::size_t i = 0; i < sidecar.segments.size(); ++i)
        if (!sidecar.segments[i].output.empty()) withOut.push_back(i);
    auto pairs = lcs(withOut, b, [&](std::size_t s, const Chunk &c) {
        return sidecar.segments[s].output == c.text;
    });
    if (pairs.empty() && !withOut.empty()) {
        r.note = "the sidecar matches nothing in this file, and is ignored";
        return r;
    }
    std::vector<long> matchOf(sidecar.segments.size(), -1);
    for (std::size_t k = 0; k < pairs.size(); ++k)
        matchOf[withOut[pairs[k].first]] = static_cast<long>(pairs[k].second);

    std::string fresh;
    bool haveFresh = false, freshFailed = false;
    std::vector<Chunk> c;
    std::vector<bool> used;
    std::set<std::string> keptKeys;
    std::vector<std::string> pieces;
    long firstFresh = -1;
    std::string freshText;

    auto freshFor = [&](const Chunk &chunk) {
        if (chunk.kind == Chunk::Beyond) {
            pieces.push_back(decodeBeyond(chunk));
            ++r.fresh;
            return;
        }
        if (chunk.kind == Chunk::Trailer) { pieces.push_back(chunk.text); return; }
        if (!haveFresh && !freshFailed) {
            if (convertFresh(fresh)) {
                haveFresh = true;
                c = Segmenter::split(fresh);
                used.assign(c.size(), false);
            } else {
                freshFailed = true;
            }
        }
        if (!haveFresh) return;
        std::string text;
        for (std::size_t i = 0; i < c.size(); ++i) {
            if (!used[i] && !chunk.key.empty() && c[i].key == chunk.key) {
                used[i] = true;
                text += c[i].text;
            }
        }
        if (text.empty()) return;
        if (firstFresh < 0) firstFresh = static_cast<long>(pieces.size());
        pieces.push_back(text);
        freshText += text;
        ++r.fresh;
    };

    std::size_t j = 0;
    for (std::size_t s = 0; s < sidecar.segments.size(); ++s) {
        const Sidecar::Segment &g = sidecar.segments[s];
        if (g.output.empty()) {
            pieces.push_back(g.source);
            keptKeys.insert(g.key);
            ++r.kept;
            continue;
        }
        if (matchOf[s] < 0) { ++r.dropped; continue; }
        for (; j < static_cast<std::size_t>(matchOf[s]); ++j) freshFor(b[j]);
        j = static_cast<std::size_t>(matchOf[s]) + 1;
        pieces.push_back(g.source);
        if (!g.source.empty()) keptKeys.insert(g.key);
        ++r.kept;
    }
    for (; j < b.size(); ++j) freshFor(b[j]);
    if (freshFailed) {
        r.note = "the edited constructs could not be converted";
        return r;
    }

    // What the fresh constructs lean on and nothing kept provides: the helpers,
    // includes and borrows the fresh conversion wrote for them.
    std::string support, top;
    if (haveFresh && firstFresh >= 0) {
        std::set<std::string> wanted;
        std::vector<std::string> ids = Segmenter::identifiers(freshText);
        wanted.insert(ids.begin(), ids.end());
        std::string assembled;
        for (std::size_t i = 0; i < pieces.size(); ++i) assembled += pieces[i];
        bool grew = true;
        std::vector<bool> taken(c.size(), false);
        while (grew) {
            grew = false;
            for (std::size_t i = 0; i < c.size(); ++i) {
                if (used[i] || taken[i] || c[i].kind != Chunk::Code) continue;
                if (c[i].key.compare(0, 5, "uses:") == 0) continue;
                if (keptKeys.count(c[i].key) || !wanted.count(c[i].key)) continue;
                taken[i] = true;
                grew = true;
                std::vector<std::string> more = Segmenter::identifiers(c[i].text);
                wanted.insert(more.begin(), more.end());
            }
        }
        for (std::size_t i = 0; i < c.size(); ++i) {
            if (taken[i]) support += c[i].text;
            bool header = c[i].kind == Chunk::Directive || c[i].key.compare(0, 5, "uses:") == 0;
            if (header && !used[i] && assembled.find(trimmed(c[i].text)) == std::string::npos) {
                bool replaced = false;
                if (c[i].key.compare(0, 5, "uses:") == 0) {
                    for (std::size_t q = 0; q < pieces.size() && !replaced; ++q) {
                        std::vector<Chunk> pc = Segmenter::split(pieces[q]);
                        if (pc.size() == 1 && pc[0].key.compare(0, 5, "uses:") == 0) {
                            pieces[q] = c[i].text;
                            replaced = true;
                        }
                    }
                }
                if (!replaced) top += c[i].text;
            }
        }
        pieces[static_cast<std::size_t>(firstFresh)] = support + pieces[static_cast<std::size_t>(firstFresh)];
    }
    r.text = top;
    for (std::size_t i = 0; i < pieces.size(); ++i) r.text += pieces[i];
    r.usable = true;
    return r;
}

std::string Keeper::restoreBeyond(const std::string &input, const std::string &converted,
                                  int &restored) {
    restored = 0;
    std::vector<Chunk> in = Segmenter::split(input), out = Segmenter::split(converted);
    std::vector<std::string> before(out.size() + 1);
    for (std::size_t i = 0; i < in.size(); ++i) {
        if (in[i].kind != Chunk::Beyond) continue;
        std::string lines = decodeBeyond(in[i]);
        if (lines.empty()) continue;
        std::size_t at = out.size();
        for (std::size_t k = i + 1; k < in.size() && at == out.size(); ++k) {
            if (in[k].key.empty() || in[k].kind == Chunk::Beyond) continue;
            for (std::size_t o = 0; o < out.size(); ++o)
                if (out[o].key == in[k].key) { at = o; break; }
        }
        before[at] += lines;
        ++restored;
    }
    if (restored == 0) return converted;
    std::string text;
    for (std::size_t o = 0; o < out.size(); ++o) text += before[o] + out[o].text;
    return text + before[out.size()];
}

}
