#ifndef C2S_C_CPRINTER_H
#define C2S_C_CPRINTER_H

#include <string>

#include "CAst.h"

namespace c2s {

// CProgram back to C89 text.
//
// The output is canonical - four-space indentation, one declaration per
// line kept as written, every literal in its original spelling - and
// compiles under cc1, which is the printer's real contract: the differential
// suite compiles what this prints and runs it against the original.
//
// Parentheses are re-derived from C's precedence table rather than
// remembered, so a printed expression may drop redundant parentheses the
// source wrote; it never drops needed ones.
class CPrinter : public CVisitor {
public:
    CPrinter();

    std::string print(CProgram &program);
    std::string printExpr(CExpr &expr);

    // One declarator's full declaration text, "int (*f)(int, int)" - shared
    // with diagnostics that want to name a declaration precisely.
    static std::string declaration(const CType &type, const std::string &name);

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
    // C's precedence, as levels this printer compares; higher binds tighter.
    enum {
        PrecComma = 1,
        PrecAssign = 2,
        PrecTernary = 3,
        PrecOr = 4,
        PrecAnd = 5,
        PrecBitOr = 6,
        PrecBitXor = 7,
        PrecBitAnd = 8,
        PrecEquality = 9,
        PrecRelational = 10,
        PrecShift = 11,
        PrecAdditive = 12,
        PrecMultiplicative = 13,
        PrecUnary = 14,
        PrecPostfix = 15
    };

    static int precedenceOfBinary(const std::string &op);

    void expr(CExpr &node, int floor);
    void stmt(CStmt &node);
    void child(CStmt &node);           // a statement under if/while/for
    void printDecl(CDeclaration &decl);
    void printInit(CInit &init);
    static std::string typeText(const CType &type, const std::string &inner);
    static std::string specifierText(const CType &type);

    void indent();

    std::string out_;
    int depth_;
    int floor_;
};

}  // namespace c2s

#endif
