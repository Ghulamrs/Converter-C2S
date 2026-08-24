#ifndef C2S_S_SPRINTER_H
#define C2S_S_SPRINTER_H

#include <string>

#include "vendor/Ast.h"

namespace c2s {

// shalimar::Program back to .shm text.
//
// Compiler-S has no unparser of its own - its back end emits assembly - so
// this is the one in the world, and it has two obligations beyond the
// grammar, both enforced by shc's parser and both silent if broken by a
// printer that reflows lines:
//
//   1. '?' and '??' must be the first token on their line, and their item
//      list runs to the end of that line. Every statement here gets a line
//      to itself, which satisfies the rule by construction.
//   2. A 'return' expression must be on the 'return''s own line.
//
// The output is canonical rather than faithful to any particular layout:
// two-space indentation, one statement per line, 'for i : 0 to n - 1' even
// where 'for i < n' was the shorter spelling. What must survive is what the
// program does, and the differential suite holds it to that.
//
// The tree may be printed before or after Compiler-S's Checker has run: the
// printer reads only what the parser wrote, never a resolved symbol, a slot,
// or an inserted Convert's target - with one exception. A Convert node prints
// as the conversion the source spells, 'int(x)', 'real(x)', 'char(x)', which
// is right both for a written conversion and for re-printing a checked tree.
class SPrinter : public shalimar::NodeVisitor {
public:
    SPrinter();

    // The whole program, in its recorded top-level order.
    std::string print(shalimar::Program &program);

    // One expression, as it would appear inside a statement. Used by
    // diagnostics that want to quote a piece of tree back.
    std::string printExpr(shalimar::Expr &expr);

    // How a double is spelled so that shc's lexer reads back the same value
    // and reads it as a real: shortest round-trip form, with a '.0' appended
    // where the digits alone would lex as an int.
    static std::string spellReal(double value);

    // NodeVisitor.
    void visit(shalimar::IntLit &) override;
    void visit(shalimar::RealLit &) override;
    void visit(shalimar::Var &) override;
    void visit(shalimar::Convert &) override;
    void visit(shalimar::Binary &) override;
    void visit(shalimar::Declare &) override;
    void visit(shalimar::Assign &) override;
    void visit(shalimar::CompoundAssign &) override;
    void visit(shalimar::Print &) override;
    void visit(shalimar::If &) override;
    void visit(shalimar::While &) override;
    void visit(shalimar::For &) override;
    void visit(shalimar::Break &) override;
    void visit(shalimar::Continue &) override;
    void visit(shalimar::Call &) override;
    void visit(shalimar::Return &) override;
    void visit(shalimar::MultiAssign &) override;
    void visit(shalimar::CallStmt &) override;
    void visit(shalimar::StrLit &) override;
    void visit(shalimar::ArrayLit &) override;
    void visit(shalimar::Blank &) override;
    void visit(shalimar::Index &) override;
    void visit(shalimar::Dim &) override;
    void visit(shalimar::Precision &) override;

private:
    // Shalimar's precedence tiers, loosest first, exactly as shc's parser
    // implements them. An operand is parenthesised when its own tier binds
    // looser than the position it stands in.
    enum Tier {
        TierOr = 1,
        TierAnd = 2,
        TierComparison = 3,
        TierAdditive = 4,
        TierMultiplicative = 5,
        TierPower = 6,
        TierPrimary = 7
    };

    static Tier tierOf(shalimar::Binary::Op op);

    void expr(shalimar::Expr &node, int floor);
    void statement(shalimar::Stmt &node);
    void block(shalimar::Block &body);
    void functionHeader(const shalimar::Prototype &proto);
    void declare(shalimar::Declare &node);

    void indent();
    void line(const std::string &text);

    std::string out_;
    int depth_;

    // The tier an expression must meet to print without parentheses; set
    // around each expr() recursion rather than passed through the visitor,
    // whose signature is fixed by Compiler-S.
    int floor_;
};

}  // namespace c2s

#endif
