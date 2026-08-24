#ifndef C2S_CONVERT_CTOS_H
#define C2S_CONVERT_CTOS_H

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "../c/CAst.h"
#include "../s/vendor/Ast.h"

namespace c2s {

class Source;
class Diagnostics;

// C89 to Shalimar: a visitor over the source-faithful C tree that builds a
// Shalimar tree, printed afterwards by SPrinter.
//
// The mirror of SToC, by design: the same visitor-over-the-source-AST shape,
// the same convertFunction / statement / expression vocabulary, the same
// tree-not-text output, and the same #BEYOND SHALIMAR marker where a
// construct has no translation - the marker interrupts the output as a
// comment quoting the original C, and the program around it still comes
// through.
//
// What is rewritten rather than refused:
//
//   - 'switch' becomes a saved selector and an if / elseif / else chain,
//     empty grouped cases joined with '|', the trailing 'break' of each
//     case dropped. Genuine fall-through is a marker.
//   - 'do { } while (c)' is peeled: the body once, then a while.
//   - Counting 'for' loops become 'for i : a to b step k'; any other shape
//     lowers to a while with the step at the body's end.
//   - Block-scoped declarations hoist to the top of the function, renamed
//     where scopes collided; C block structure flattens away.
//   - 'x ? a : b' as the whole right side of an assignment becomes an
//     if / else; '++', '--' and the compound assignments become '+:', '-:'
//     or spelled-out assignments; '!x' becomes 'x = 0'.
//   - printf with a literal format becomes '?' / '??' items; puts and
//     putchar likewise. The '?' item spacing is Shalimar's own, so printed
//     output matches up to whitespace, not byte for byte.
//   - '&&' and '||' become '&' and '|' when the right side is pure - no
//     calls, no indexing, no division - because Shalimar's forms evaluate
//     both sides; otherwise they are markers.
//
// A char literal becomes its code point, wrapped in char() where the other
// side of the expression is char; a hex or octal literal becomes its value
// in decimal; casts among int, double and char become int(), real(),
// char(). Everything pointer-shaped, struct-shaped, unsigned, long, or
// preprocessor-born is a marker naming what it was.
class CToS : public CVisitor {
public:
    CToS(const Source &source, Diagnostics &diagnostics);

    std::unique_ptr<shalimar::Program> convert(CProgram &program);

    int beyondCount() const { return beyondCount_; }

    // Symmetry with SToC: Shalimar needs no emitted runtime, so this is
    // always empty, and no include lines exist to need.
    std::string preamble() const { return std::string(); }

    // CVisitor - expressions set expr_, statements append to block_.
    void visit(CIntLit &) override;
    void visit(CFloatLit &) override;
    void visit(CCharLit &) override;
    void visit(CStringLit &) override;
    void visit(CIdent &) override;
    void visit(CUnary &) override;
    void visit(CBinary &) override;
    void visit(CAssign &) override;
    void visit(CTernary &) override;
    void visit(CCall &) override;
    void visit(CIndex &) override;
    void visit(CMember &) override;
    void visit(CCast &) override;
    void visit(CSizeof &) override;
    void visit(CComma &) override;
    void visit(CExprStmt &) override;
    void visit(CEmpty &) override;
    void visit(CCompound &) override;
    void visit(CIf &) override;
    void visit(CWhile &) override;
    void visit(CDoWhile &) override;
    void visit(CFor &) override;
    void visit(CSwitch &) override;
    void visit(CCase &) override;
    void visit(CBreak &) override;
    void visit(CContinue &) override;
    void visit(CReturn &) override;
    void visit(CGoto &) override;
    void visit(CLabel &) override;
    void visit(CDeclStmt &) override;
    void visit(CBeyond &) override;

private:
    // What this converter knows about one C name.
    struct Info {
        std::string sName;                 // after renaming
        const shalimar::Type *type = nullptr;
        int rank = 0;
        bool isChar = false;               // for char-context decisions
    };

    // ---- the shared converter vocabulary (same names in SToC) ----
    void convertFunction(CFunctionDef &fn);
    void statement(CStmt &node);
    shalimar::ExprPtr expression(CExpr &node);
    void block(CStmt &node, shalimar::Block *into);
    void markBeyond(std::size_t offset, const std::string &reason);

    // ---- helpers of this direction ----
    const shalimar::Type *scalarS(const CType &type, bool *lossy) const;
    std::string rename(const std::string &name);
    const Info *lookup(const std::string &name) const;
    void declareLocal(CDeclaration &decl, bool atTop);
    bool isPure(CExpr &node) const;
    bool isCharContext(CExpr &other) const;
    shalimar::ExprPtr charWrap(shalimar::ExprPtr value);
    void lowerPrintf(CCall &call);
    bool lowerCountingFor(CFor &node);
    void lowerSwitch(CSwitch &node);
    void hoistDeclarations(CStmt &node, shalimar::Block *top);
    void convertTopDeclaration(CDeclaration &decl);
    std::size_t declOffset(CDeclaration &decl) const;
    int lineOf(std::size_t offset) const;
    std::vector<std::string> sourceLinesAt(std::size_t offset) const;

    const Source &source_;
    Diagnostics &diagnostics_;

    std::unique_ptr<shalimar::Program> program_;
    shalimar::Block *block_ = nullptr;
    shalimar::ExprPtr expr_;
    bool currentIsMain_ = false;
    int loopDepth_ = 0;

    // Name scopes: each maps a C name to its Info; lookups walk outward.
    std::vector<std::map<std::string, Info>> scopes_;
    std::set<std::string> usedNames_;      // every Shalimar name handed out
    std::set<std::string> knownFunctions_; // defined in this file
    int beyondCount_ = 0;
    int tempCount_ = 0;
};

}  // namespace c2s

#endif
