// The checker: types the whole program before anything runs.
//
// It behaves unlike the lexer and the parser and that is its point. **It does
// not stop at the first problem.** It types everything it can, records every
// diagnostic it finds, and only then says whether the program may run. That
// is why several messages can appear at once, and why warnings appear for
// programs that run anyway.
//
// It also rewrites. A conversion the language performs silently is inserted
// here as a node, so that by the time the code generator sees the tree every
// value has one type and nothing is implicit. And it measures: how many names
// a function declares and how deeply its expressions nest are both answered
// by this walk, because it is the only pass that sees both.
#pragma once

#include "Ast.h"
#include "Diag.h"

#include <map>
#include <string>
#include <vector>

namespace shalimar {

// The names visible at one point in a function. Blocks nest, so a variable
// first assigned inside an 'if' is gone after it - which is the app's rule,
// and not an obvious one, since a declaration may only appear at the top of a
// function body and so always outlives every block.
class Scope {
public:
    void push() { levels_.push_back(Level()); }
    void pop() { levels_.pop_back(); }
    void clear() { levels_.clear(); }

    void define(const std::string &name, const Symbol *symbol) {
        levels_.back()[name] = symbol;
    }

    // Innermost first; null when the name is not in scope at all.
    const Symbol *lookup(const std::string &name) const {
        for (size_t i = levels_.size(); i-- > 0;) {
            Level::const_iterator found = levels_[i].find(name);
            if (found != levels_[i].end()) return found->second;
        }
        return nullptr;
    }

    bool definedHere(const std::string &name) const {
        return !levels_.empty() && levels_.back().count(name) != 0;
    }

private:
    using Level = std::map<std::string, const Symbol *>;
    std::vector<Level> levels_;
};

class Checker : public NodeVisitor {
public:
    explicit Checker(Diagnostics &diagnostics) : diag_(diagnostics) {}

    bool check(Program &program);

    void visit(IntLit &node) override;
    void visit(RealLit &node) override;
    void visit(StrLit &node) override;
    void visit(ArrayLit &node) override;
    void visit(Blank &node) override;
    void visit(Var &node) override;
    void visit(Index &node) override;
    void visit(Dim &node) override;
    void visit(Precision &node) override;
    void visit(Convert &node) override;
    void visit(Binary &node) override;
    void visit(Call &node) override;

    void visit(Declare &node) override;
    void visit(Assign &node) override;
    void visit(CompoundAssign &node) override;
    void visit(MultiAssign &node) override;
    void visit(CallStmt &node) override;
    void visit(Return &node) override;
    void visit(Print &node) override;
    void visit(If &node) override;
    void visit(While &node) override;
    void visit(For &node) override;
    void visit(Break &node) override;
    void visit(Continue &node) override;

private:
    Diagnostics &diag_;
    Program *program_ = nullptr;
    int line_ = 0;
    int unit_ = 0;
    Function *function_ = nullptr;
    Scope scope_;
    int strings_ = 0;

    // Globals as the walk reaches them, and every global with the line it is
    // declared on until it is reached. Without the second, a name used above
    // its declaration reports as 'Undefined variable' - true, but it sends
    // the reader looking for a name that is in the file, spelled correctly, a
    // few lines further down.
    std::map<std::string, const Symbol *> globals_;
    std::map<std::string, int> laterGlobals_;
    bool inGlobalScope_ = false;

    Symbol *declareName(const std::string &name, const Type *type);
    const Symbol *lookup(const std::string &name) const;
    void reportUndefined(const std::string &name);

    void check(Function &function);
    void check(Stmt &statement);

    const Type *typeOf(ExprPtr &expr);
    void coerce(ExprPtr &expr, const Type *to);
    const Type *common(const Type *a, const Type *b) const;

    void checkCondition(ExprPtr &expr);
    void checkBlock(Block &body);

    // The type an array literal has, worked out from its shape and from the
    // first slot that holds something. A literal blank all the way down has a
    // shape but no type, which is why this can answer null.
    const Type *literalType(ArrayLit &node);
    void coerceLiteral(ArrayLit &node, const Type *arrayType);

    bool constantNumber(const Expr &expr, double &value) const;
    void warnIfLoopNeverRuns(For &node);
    static std::string number(double value);
    static bool alwaysReturns(const Block &body);

    // A name that cannot be written: 'pi' and 'e'.
    bool refuseConstant(const std::string &name, const char *what);

};

}  // namespace shalimar
