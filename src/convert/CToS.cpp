#include "CToS.h"

#include <cctype>
#include <cstdio>

#include "../Diagnostics.h"
#include "../Source.h"
#include "../s/SBeyond.h"
#include "../s/vendor/Builtin.h"

namespace c2s {

namespace {

// A name Shalimar cannot take: its keywords (matched case-insensitively by
// shc's lexer), the builtins, the two constants, and 'prec'.
bool isShalimarReserved(const std::string &name) {
    static const char *const words[] = {
        "if", "elseif", "else", "while", "for", "to", "step", "fun", "return",
        "break", "continue", "int", "real", "char", "prec", "pi", "e",
        "abs", "sqrt", "log", "exp", "hypot", "sin", "cos", "tan", "asin",
        "acos", "atan", "atan2", "pow", "round", "ceil", "floor", "trunc",
        "max", "min", "len"
    };
    std::string lower;
    for (std::size_t i = 0; i < name.size(); ++i) {
        lower += static_cast<char>(std::tolower(static_cast<unsigned char>(name[i])));
    }
    for (std::size_t i = 0; i < sizeof words / sizeof words[0]; ++i) {
        if (lower == words[i]) return true;
    }
    return false;
}

// The C89 math functions Shalimar answers with a builtin of the same
// meaning. 'fabs' changes its name; 'fmod' becomes the '%' operator.
const char *builtinFor(const std::string &name) {
    static const char *const same[] = {
        "sqrt", "log", "exp", "sin", "cos", "tan", "asin", "acos", "atan",
        "atan2", "pow", "ceil", "floor", "abs"
    };
    for (std::size_t i = 0; i < sizeof same / sizeof same[0]; ++i) {
        if (name == same[i]) return same[i];
    }
    if (name == "fabs") return "abs";
    return nullptr;
}

shalimar::ExprPtr sInt(long long value) {
    return shalimar::ExprPtr(new shalimar::IntLit(static_cast<int32_t>(value)));
}


// A read-only walk over a C subtree, answering two questions at once: how
// far into the source the subtree reaches, and every place a given name is
// read. Both together decide whether a for loop's counter is still live
// after the loop ends - see counterEscapes.
class NameScan : public CVisitor {
public:
    explicit NameScan(std::string name) : name_(std::move(name)) {}

    void run(CStmt &node) { node.accept(*this); }

    std::size_t reach() const { return reach_; }
    const std::vector<std::size_t> &uses() const { return uses_; }

    void visit(CIntLit &n) override { note(n); }
    void visit(CFloatLit &n) override { note(n); }
    void visit(CCharLit &n) override { note(n); }
    void visit(CStringLit &n) override { note(n); }
    void visit(CIdent &n) override {
        note(n);
        if (n.name() == name_) uses_.push_back(n.offset());
    }
    void visit(CUnary &n) override { note(n); n.operand().accept(*this); }
    void visit(CBinary &n) override {
        note(n); n.lhs().accept(*this); n.rhs().accept(*this);
    }
    void visit(CAssign &n) override {
        note(n); n.target().accept(*this); n.value().accept(*this);
    }
    void visit(CTernary &n) override {
        note(n); n.cond().accept(*this);
        n.thenArm().accept(*this); n.elseArm().accept(*this);
    }
    void visit(CCall &n) override {
        note(n); n.callee().accept(*this);
        std::vector<CExprPtr> &args = n.args();
        for (std::size_t i = 0; i < args.size(); ++i) args[i]->accept(*this);
    }
    void visit(CIndex &n) override {
        note(n); n.base().accept(*this); n.index().accept(*this);
    }
    void visit(CMember &n) override { note(n); n.object().accept(*this); }
    void visit(CCast &n) override { note(n); n.operand().accept(*this); }
    void visit(CSizeof &n) override {
        note(n);
        if (n.operand() != nullptr) n.operand()->accept(*this);
    }
    void visit(CComma &n) override {
        note(n); n.left().accept(*this); n.right().accept(*this);
    }
    void visit(CExprStmt &n) override { note(n); n.expr().accept(*this); }
    void visit(CEmpty &n) override { note(n); }
    void visit(CCompound &n) override {
        note(n);
        std::vector<CStmtPtr> &body = n.body();
        for (std::size_t i = 0; i < body.size(); ++i) body[i]->accept(*this);
    }
    void visit(CIf &n) override {
        note(n); n.cond().accept(*this); n.thenArm().accept(*this);
        if (n.elseArm() != nullptr) n.elseArm()->accept(*this);
    }
    void visit(CWhile &n) override {
        note(n); n.cond().accept(*this); n.body().accept(*this);
    }
    void visit(CDoWhile &n) override {
        note(n); n.body().accept(*this); n.cond().accept(*this);
    }
    void visit(CFor &n) override {
        note(n);
        if (n.init() != nullptr) n.init()->accept(*this);
        if (n.cond() != nullptr) n.cond()->accept(*this);
        if (n.step() != nullptr) n.step()->accept(*this);
        n.body().accept(*this);
    }
    void visit(CSwitch &n) override {
        note(n); n.cond().accept(*this); n.body().accept(*this);
    }
    void visit(CCase &n) override {
        note(n);
        if (n.value() != nullptr) n.value()->accept(*this);
        n.body().accept(*this);
    }
    void visit(CBreak &n) override { note(n); }
    void visit(CContinue &n) override { note(n); }
    void visit(CReturn &n) override {
        note(n);
        if (n.value() != nullptr) n.value()->accept(*this);
    }
    void visit(CGoto &n) override { note(n); }
    void visit(CLabel &n) override { note(n); n.body().accept(*this); }
    void visit(CDeclStmt &n) override {
        note(n);
        std::vector<CDeclaration::Declarator> &declarators =
            n.decl().declarators();
        for (std::size_t i = 0; i < declarators.size(); ++i) {
            if (declarators[i].offset > reach_) reach_ = declarators[i].offset;
            if (declarators[i].init != nullptr) scanInit(*declarators[i].init);
        }
    }
    void visit(CBeyond &n) override { note(n); }

private:
    void note(CNode &n) { if (n.offset() > reach_) reach_ = n.offset(); }
    void scanInit(CInit &init) {
        if (!init.isList()) {
            if (init.expr() != nullptr) init.expr()->accept(*this);
            return;
        }
        std::vector<CInit> &items = init.items();
        for (std::size_t i = 0; i < items.size(); ++i) scanInit(items[i]);
    }

    std::string name_;
    std::size_t reach_ = 0;
    std::vector<std::size_t> uses_;
};

}  // namespace

CToS::CToS(const Source &source, Diagnostics &diagnostics)
    : source_(source) {
    (void)diagnostics;
}

// ------------------------------------------------------------------ helpers

int CToS::lineOf(std::size_t offset) const {
    return source_.locate(offset).line();
}

std::vector<std::string> CToS::sourceLinesAt(std::size_t offset) const {
    std::vector<std::string> lines;
    const int line = lineOf(offset);
    if (line > 0) {
        const std::string text = source_.line(line);
        if (!text.empty()) lines.push_back(text);
    }
    return lines;
}

void CToS::markBeyond(std::size_t offset, const std::string &reason) {
    ++beyondCount_;
    shalimar::StmtPtr marker(new SBeyondStmt(reason, sourceLinesAt(offset),
                                             lineOf(offset)));
    // A refusal outside any function body - a struct, a file-scope
    // declaration - has to appear too, or the count says six and a reader
    // looking for six finds five. Program keeps its top-level order, so the
    // marker lands there and prints where the construct stood.
    if (block_ != nullptr) {
        block_->push_back(std::move(marker));
    } else if (program_ != nullptr) {
        program_->addGlobal(std::move(marker));
    }
}

const shalimar::Type *CToS::scalarS(const CType &type, bool *lossy) const {
    *lossy = false;
    switch (type.kind()) {
        case CType::Kind::Int:
            if (type.isUnsigned() || type.isLong() || type.isShort()) {
                *lossy = true;
            }
            return shalimar::Type::intType();
        case CType::Kind::Double:
            if (type.isLong()) *lossy = true;      // long double
            return shalimar::Type::realType();
        case CType::Kind::Float:
            return shalimar::Type::realType();     // widened, harmlessly
        case CType::Kind::Char:
            if (type.isUnsigned() || type.isSignedExplicit()) *lossy = true;
            return shalimar::Type::charType();
        default:
            *lossy = true;
            return nullptr;
    }
}

std::string CToS::rename(const std::string &name) {
    std::string candidate = name;
    if (isShalimarReserved(candidate)) candidate += "_v";
    while (usedNames_.count(candidate) != 0) candidate += "_2";
    usedNames_.insert(candidate);
    return candidate;
}

const CToS::Info *CToS::lookup(const std::string &name) const {
    for (std::size_t i = scopes_.size(); i > 0; --i) {
        std::map<std::string, Info>::const_iterator it = scopes_[i - 1].find(name);
        if (it != scopes_[i - 1].end()) return &it->second;
    }
    return nullptr;
}

bool CToS::isPure(CExpr &node) const {
    // Pure enough to evaluate unconditionally: no calls, no indexing, no
    // division, no assignment, no increment. What remains cannot fault and
    // cannot be observed happening.
    if (dynamic_cast<CIntLit *>(&node) != nullptr) return true;
    if (dynamic_cast<CFloatLit *>(&node) != nullptr) return true;
    if (dynamic_cast<CCharLit *>(&node) != nullptr) return true;
    if (dynamic_cast<CIdent *>(&node) != nullptr) return true;
    if (CUnary *unary = dynamic_cast<CUnary *>(&node)) {
        if (unary->op() == "-" || unary->op() == "+" || unary->op() == "!") {
            return isPure(unary->operand());
        }
        return false;
    }
    if (CBinary *binary = dynamic_cast<CBinary *>(&node)) {
        const std::string &op = binary->op();
        if (op == "/" || op == "%") return false;
        return isPure(binary->lhs()) && isPure(binary->rhs());
    }
    return false;
}

bool CToS::isCharContext(CExpr &other) const {
    if (CIdent *identifier = dynamic_cast<CIdent *>(&other)) {
        const Info *info = lookup(identifier->name());
        return info != nullptr && info->isChar && info->rank == 0;
    }
    if (CIndex *index = dynamic_cast<CIndex *>(&other)) {
        CExpr *walk = &index->base();
        while (CIndex *deeper = dynamic_cast<CIndex *>(walk)) walk = &deeper->base();
        if (CIdent *base = dynamic_cast<CIdent *>(walk)) {
            const Info *info = lookup(base->name());
            return info != nullptr && info->isChar;
        }
    }
    return false;
}

shalimar::ExprPtr CToS::charWrap(shalimar::ExprPtr value) {
    return shalimar::ExprPtr(
        new shalimar::Convert(std::move(value), shalimar::Type::charType()));
}

shalimar::ExprPtr CToS::intWrap(shalimar::ExprPtr value) {
    return shalimar::ExprPtr(
        new shalimar::Convert(std::move(value), shalimar::Type::intType()));
}

// -------------------------------------------------------------- expressions

shalimar::ExprPtr CToS::expression(CExpr &node) {
    shalimar::ExprPtr saved = std::move(expr_);
    expr_.reset();
    node.accept(*this);
    shalimar::ExprPtr result = std::move(expr_);
    expr_ = std::move(saved);
    if (result == nullptr) result = sInt(0);   // after a marker; keeps the tree whole
    return result;
}

void CToS::visit(CIntLit &node) {
    if (node.value() > 2147483647LL || node.value() < -2147483648LL ||
        node.isLong()) {
        markBeyond(node.offset(), "an integer beyond Shalimar's 32-bit int");
        expr_.reset();
        return;
    }
    // A hex or octal spelling becomes its value in decimal - Shalimar has
    // only decimal - and a fitting U suffix simply drops.
    expr_ = sInt(node.value());
}

void CToS::visit(CFloatLit &node) {
    expr_.reset(new shalimar::RealLit(node.value()));
}

void CToS::visit(CCharLit &node) {
    // Its code point; the assignment and comparison paths wrap it in
    // char() when the other side is a char.
    expr_ = sInt(node.value());
}

void CToS::visit(CStringLit &node) {
    const std::string &text = node.text();
    for (std::size_t i = 0; i < text.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if (c == '"' || c == '\n' || c < 32) {
            markBeyond(node.offset(),
                       "a string with characters Shalimar cannot spell - it has "
                       "no escapes");
            expr_.reset();
            return;
        }
    }
    expr_.reset(new shalimar::StrLit(text));
}

void CToS::visit(CIdent &node) {
    const Info *info = lookup(node.name());
    expr_.reset(new shalimar::Var(info != nullptr ? info->sName : node.name()));
}

void CToS::visit(CUnary &node) {
    const std::string &op = node.op();

    if (op == "-" && node.prefix()) {
        // Fold into the literal the way shc's parser does; anything else is
        // a subtraction from zero, which prints as '0 - x'.
        if (CIntLit *lit = dynamic_cast<CIntLit *>(&node.operand())) {
            expr_ = sInt(-lit->value());
            return;
        }
        if (CFloatLit *lit = dynamic_cast<CFloatLit *>(&node.operand())) {
            expr_.reset(new shalimar::RealLit(-lit->value()));
            return;
        }
        expr_.reset(new shalimar::Binary(shalimar::Binary::Op::Subtract,
                                         sInt(0), expression(node.operand())));
        return;
    }
    if (op == "+" && node.prefix()) {
        expr_ = expression(node.operand());
        return;
    }
    if (op == "!" && node.prefix()) {
        // Shalimar has no '!'; 'x = 0' answers the same 1 or 0.
        expr_.reset(new shalimar::Binary(shalimar::Binary::Op::Equal,
                                         expression(node.operand()), sInt(0)));
        return;
    }
    if (op == "++" || op == "--") {
        markBeyond(node.offset(),
                   std::string("'") + op + "' inside an expression - only the "
                   "statement forms become '+:' and '-:'");
        expr_.reset();
        return;
    }
    if (op == "~") {
        markBeyond(node.offset(), "'~' - Shalimar has no bitwise operators");
        expr_.reset();
        return;
    }
    if (op == "&" || op == "*") {
        markBeyond(node.offset(),
                   std::string("'") + op + "' - Shalimar has no pointers");
        expr_.reset();
        return;
    }
    markBeyond(node.offset(), "'" + op + "'");
    expr_.reset();
}

void CToS::visit(CBinary &node) {
    using Op = shalimar::Binary::Op;
    const std::string &op = node.op();

    if (op == "&&" || op == "||") {
        // Shalimar's '&' and '|' evaluate both sides. Safe only when the
        // right side cannot fault and does nothing observable.
        if (!isPure(node.rhs())) {
            markBeyond(node.offset(),
                       std::string("'") + op + "' whose right side is not pure - "
                       "Shalimar's form evaluates both sides");
            expr_.reset();
            return;
        }
        expr_.reset(new shalimar::Binary(op == "&&" ? Op::And : Op::Or,
                                         expression(node.lhs()),
                                         expression(node.rhs())));
        return;
    }

    if (op == "&" || op == "|" || op == "^" || op == "<<" || op == ">>") {
        markBeyond(node.offset(),
                   "'" + op + "' - Shalimar has no bitwise operators");
        expr_.reset();
        return;
    }

    Op mapped;
    if (op == "+") mapped = Op::Add;
    else if (op == "-") mapped = Op::Subtract;
    else if (op == "*") mapped = Op::Multiply;
    else if (op == "/") mapped = Op::Divide;
    else if (op == "%") mapped = Op::Modulus;
    else if (op == "==") mapped = Op::Equal;
    else if (op == "!=") mapped = Op::NotEqual;
    else if (op == "<") mapped = Op::Less;
    else if (op == ">") mapped = Op::Greater;
    else if (op == "<=") mapped = Op::LessEqual;
    else if (op == ">=") mapped = Op::GreaterEqual;
    else {
        markBeyond(node.offset(), "'" + op + "'");
        expr_.reset();
        return;
    }

    shalimar::ExprPtr lhs = expression(node.lhs());
    shalimar::ExprPtr rhs = expression(node.rhs());

    // C's integer promotion, written out. The moment a char is used
    // arithmetically C makes it an int and says nothing; Shalimar has no
    // such rule and refuses '+' on a char outright, so the promotion has to
    // appear in the output as int(). Getting this wrong is not only a
    // compile error: '%d' of an unpromoted char is accepted by shc and
    // prints the character where C printed its code.
    //
    // A char array is left alone - rank is why isCharContext says no to one
    // - because '+' on those is Shalimar's string concatenation, which is
    // a different operator wearing the same spelling.
    const bool comparison = mapped == Op::Equal || mapped == Op::NotEqual ||
                            mapped == Op::Less || mapped == Op::Greater ||
                            mapped == Op::LessEqual || mapped == Op::GreaterEqual;
    const bool lhsChar = isCharContext(node.lhs());
    const bool rhsChar = isCharContext(node.rhs());
    if (comparison) {
        // A char literal against a char stays a char comparison - that is
        // char against char, which Shalimar does allow, and it reads better
        // than two codes.
        if (dynamic_cast<CCharLit *>(&node.rhs()) != nullptr && lhsChar) {
            rhs = charWrap(std::move(rhs));
        } else if (dynamic_cast<CCharLit *>(&node.lhs()) != nullptr && rhsChar) {
            lhs = charWrap(std::move(lhs));
        } else if (lhsChar != rhsChar) {
            // A char against a plain number: C compares the codes.
            if (lhsChar) lhs = intWrap(std::move(lhs));
            else rhs = intWrap(std::move(rhs));
        }
    } else {
        if (lhsChar) lhs = intWrap(std::move(lhs));
        if (rhsChar) rhs = intWrap(std::move(rhs));
    }

    expr_.reset(new shalimar::Binary(mapped, std::move(lhs), std::move(rhs)));
}

void CToS::visit(CAssign &node) {
    // Assignment as a value - 'a = b = c', 'while ((x = f()))' - has no
    // Shalimar expression; the statement path unchains the simple case
    // before coming here.
    markBeyond(node.offset(), "assignment used as a value");
    expr_.reset();
}

void CToS::visit(CTernary &node) {
    markBeyond(node.offset(),
               "'?:' inside an expression - it becomes if / else only where "
               "there is a statement to expand into, which is 'x = c ? a : b' "
               "and 'return c ? a : b'");
    expr_.reset();
}

void CToS::visit(CCall &node) {
    CIdent *callee = dynamic_cast<CIdent *>(&node.callee());
    if (callee == nullptr) {
        markBeyond(node.offset(), "a call through something not a plain name");
        expr_.reset();
        return;
    }
    const std::string &name = callee->name();

    if (name == "fmod" && node.args().size() == 2) {
        expr_.reset(new shalimar::Binary(shalimar::Binary::Op::Modulus,
                                         expression(*node.args()[0]),
                                         expression(*node.args()[1])));
        return;
    }

    const char *builtin = builtinFor(name);
    std::string sName;
    if (builtin != nullptr) {
        sName = builtin;
    } else if (knownFunctions_.count(name) != 0) {
        const Info *info = lookup(name);
        sName = info != nullptr ? info->sName : name;
    } else {
        markBeyond(node.offset(),
                   "'" + name + "' - not defined in this file, and not one of "
                   "Shalimar's twenty builtins");
        expr_.reset();
        return;
    }

    std::unique_ptr<shalimar::Call> call(
        new shalimar::Call(sName, lineOf(node.offset())));
    std::vector<CExprPtr> &args = node.args();
    for (std::size_t i = 0; i < args.size(); ++i) {
        call->add(expression(*args[i]));
    }
    expr_.reset(call.release());
}

void CToS::visit(CIndex &node) {
    expr_.reset(new shalimar::Index(expression(node.base()),
                                    expression(node.index())));
}

void CToS::visit(CMember &node) {
    markBeyond(node.offset(),
               std::string("'") + (node.arrow() ? "->" : ".") + node.name() +
               "' - Shalimar has no structs");
    expr_.reset();
}

void CToS::visit(CCast &node) {
    bool lossy = false;
    const shalimar::Type *to = scalarS(node.type(), &lossy);
    if (to == nullptr || lossy) {
        markBeyond(node.offset(),
                   "a cast to " + node.type().describe() +
                   " - only int, real and char exist");
        expr_.reset();
        return;
    }
    expr_.reset(new shalimar::Convert(expression(node.operand()), to));
}

void CToS::visit(CSizeof &node) {
    markBeyond(node.offset(), "'sizeof' - Shalimar has no size notion; arrays "
                              "answer .row, .col and .dim(n)");
    expr_.reset();
}

void CToS::visit(CComma &node) {
    markBeyond(node.offset(), "the ',' operator");
    expr_.reset();
}

void CToS::visit(CBeyond &) {}    // never appears in a parsed C tree

// --------------------------------------------------------------- statements

void CToS::statement(CStmt &node) {
    node.accept(*this);
}

void CToS::block(CStmt &node, shalimar::Block *into) {
    shalimar::Block *saved = block_;
    block_ = into;
    if (CCompound *compound = dynamic_cast<CCompound *>(&node)) {
        scopes_.push_back(std::map<std::string, Info>());
        std::vector<CStmtPtr> &body = compound->body();
        for (std::size_t i = 0; i < body.size(); ++i) statement(*body[i]);
        scopes_.pop_back();
    } else {
        statement(node);
    }
    block_ = saved;
}

void CToS::visit(CEmpty &) {}

void CToS::visit(CExprStmt &node) {
    CExpr &e = node.expr();

    // The statement-only rewrites, tried in order of specificity.

    if (CUnary *unary = dynamic_cast<CUnary *>(&e)) {
        if (unary->op() == "++" || unary->op() == "--") {
            shalimar::ExprPtr target = expression(unary->operand());
            block_->push_back(shalimar::StmtPtr(new shalimar::CompoundAssign(
                std::move(target), unary->op() == "++", sInt(1),
                lineOf(node.offset()))));
            return;
        }
    }

    if (CAssign *assign = dynamic_cast<CAssign *>(&e)) {
        const std::string &op = assign->op();

        // 'x = c ? a : b' becomes if / else writing x twice.
        if (op == "=") {
            if (CTernary *ternary = dynamic_cast<CTernary *>(&assign->value())) {
                std::unique_ptr<shalimar::If> branch(
                    new shalimar::If(lineOf(node.offset())));
                shalimar::Block thenBody;
                thenBody.push_back(shalimar::StmtPtr(new shalimar::Assign(
                    expression(assign->target()), expression(ternary->thenArm()),
                    lineOf(node.offset()))));
                shalimar::Block elseBody;
                elseBody.push_back(shalimar::StmtPtr(new shalimar::Assign(
                    expression(assign->target()), expression(ternary->elseArm()),
                    lineOf(node.offset()))));
                branch->addBranch(expression(ternary->cond()), std::move(thenBody));
                branch->setElse(std::move(elseBody));
                block_->push_back(shalimar::StmtPtr(branch.release()));
                return;
            }

            // 'a = b = c' unchains right to left.
            if (CAssign *inner = dynamic_cast<CAssign *>(&assign->value())) {
                if (inner->op() == "=") {
                    // Convert the inner assignment first, then assign its
                    // target to the outer target.
                    shalimar::ExprPtr innerTarget = expression(inner->target());
                    shalimar::ExprPtr innerValue = expression(inner->value());
                    block_->push_back(shalimar::StmtPtr(new shalimar::Assign(
                        std::move(innerTarget), std::move(innerValue),
                        lineOf(node.offset()))));
                    block_->push_back(shalimar::StmtPtr(new shalimar::Assign(
                        expression(assign->target()), expression(inner->target()),
                        lineOf(node.offset()))));
                    return;
                }
            }

            const int before = beyondCount_;
            shalimar::ExprPtr value = expression(assign->value());
            // A char literal stored into a char lands as char(n).
            if (dynamic_cast<CCharLit *>(&assign->value()) != nullptr &&
                isCharContext(assign->target())) {
                value = charWrap(std::move(value));
            }
            shalimar::ExprPtr target = expression(assign->target());
            // A marker spoke for part of this statement; the placeholder
            // that kept the walk alive must not become a wrong assignment.
            if (beyondCount_ != before) return;
            block_->push_back(shalimar::StmtPtr(new shalimar::Assign(
                std::move(target), std::move(value), lineOf(node.offset()))));
            return;
        }

        if (op == "+=" || op == "-=") {
            const int before = beyondCount_;
            shalimar::ExprPtr target = expression(assign->target());
            shalimar::ExprPtr value = expression(assign->value());
            if (beyondCount_ != before) return;
            block_->push_back(shalimar::StmtPtr(new shalimar::CompoundAssign(
                std::move(target), op == "+=", std::move(value),
                lineOf(node.offset()))));
            return;
        }
        if (op == "*=" || op == "/=" || op == "%=") {
            const shalimar::Binary::Op mapped =
                op == "*=" ? shalimar::Binary::Op::Multiply
                           : op == "/=" ? shalimar::Binary::Op::Divide
                                        : shalimar::Binary::Op::Modulus;
            const int before = beyondCount_;
            shalimar::ExprPtr rhs(new shalimar::Binary(
                mapped, expression(assign->target()), expression(assign->value())));
            shalimar::ExprPtr target = expression(assign->target());
            if (beyondCount_ != before) return;
            block_->push_back(shalimar::StmtPtr(new shalimar::Assign(
                std::move(target), std::move(rhs), lineOf(node.offset()))));
            return;
        }
        markBeyond(node.offset(),
                   "'" + op + "' - Shalimar has no bitwise operators");
        return;
    }

    if (CCall *call = dynamic_cast<CCall *>(&e)) {
        CIdent *callee = dynamic_cast<CIdent *>(&call->callee());
        if (callee != nullptr) {
            const std::string &name = callee->name();
            if (name == "printf" || name == "puts" || name == "putchar") {
                lowerPrintf(*call);
                return;
            }
        }
        shalimar::ExprPtr converted = expression(e);
        if (dynamic_cast<shalimar::Call *>(converted.get()) != nullptr) {
            block_->push_back(shalimar::StmtPtr(new shalimar::CallStmt(
                std::move(converted), lineOf(node.offset()))));
        }
        return;
    }

    // Any other expression statement computes and discards; without side
    // effects it means nothing, and Shalimar cannot say it.
    markBeyond(node.offset(), "an expression statement with no effect Shalimar "
                              "can express");
}

void CToS::visit(CDeclStmt &node) {
    // The Declare itself was emitted by the hoist, at the top of the
    // function. Two things are left to do here, and both belong at the point
    // in the block where the declaration actually stood: bind the name, and
    // run any initialiser.
    CDeclaration &decl = node.decl();
    std::vector<CDeclaration::Declarator> &declarators = decl.declarators();
    for (std::size_t i = 0; i < declarators.size(); ++i) {
        CDeclaration::Declarator &declarator = declarators[i];

        // Binding here is what makes a shadow a shadow: the name enters the
        // innermost scope at the point C says it becomes visible, and goes
        // again when block() pops that scope. A declarator the hoist could
        // not type has no entry, and was marked beyond there.
        std::map<std::size_t, Info>::const_iterator found =
            hoisted_.find(declarator.offset);
        if (found == hoisted_.end()) continue;
        // Bound one at a time, in order, so 'int a = 1, b = a;' sees the 'a'
        // it just declared rather than an outer one.
        scopes_.back()[declarator.name] = found->second;
        const Info &info = found->second;

        if (declarator.init == nullptr) continue;
        if (declarator.init->isList()) continue;   // carried by the hoisted Declare
        if (info.rank != 0) continue;              // arrays init at the top

        shalimar::ExprPtr value = expression(*declarator.init->expr());
        if (dynamic_cast<CCharLit *>(declarator.init->expr()) != nullptr &&
            info.isChar) {
            value = charWrap(std::move(value));
        }
        block_->push_back(shalimar::StmtPtr(new shalimar::Assign(
            shalimar::ExprPtr(new shalimar::Var(info.sName)), std::move(value),
            lineOf(node.offset()))));
    }
}

void CToS::visit(CIf &node) {
    // 'else if' chains flatten into elseif branches - Shalimar's 'elseif'
    // is one keyword and a nested if inside else is not grammatical.
    std::unique_ptr<shalimar::If> branch(new shalimar::If(lineOf(node.offset())));

    CIf *walk = &node;
    for (;;) {
        shalimar::ExprPtr cond = expression(walk->cond());
        shalimar::Block body;
        block(walk->thenArm(), &body);
        branch->addBranch(std::move(cond), std::move(body));

        CStmt *elseArm = walk->elseArm();
        if (elseArm == nullptr) break;
        if (CIf *chained = dynamic_cast<CIf *>(elseArm)) {
            walk = chained;
            continue;
        }
        shalimar::Block elseBody;
        block(*elseArm, &elseBody);
        branch->setElse(std::move(elseBody));
        break;
    }
    block_->push_back(shalimar::StmtPtr(branch.release()));
}

void CToS::visit(CWhile &node) {
    shalimar::ExprPtr cond = expression(node.cond());
    shalimar::Block body;
    ++loopDepth_;
    block(node.body(), &body);
    --loopDepth_;
    block_->push_back(shalimar::StmtPtr(new shalimar::While(
        std::move(cond), std::move(body), lineOf(node.offset()))));
}

namespace {

// Does this statement subtree contain a break or continue that would bind
// to the ENCLOSING loop - i.e. not shielded by a nested loop or switch?
class FindsLoopJump : public CVisitor {
public:
    bool found = false;
    bool findContinueOnly = false;

    void visit(CBreak &) override { if (!findContinueOnly) found = true; }
    void visit(CContinue &) override { found = true; }
    void visit(CWhile &) override {}       // a nested loop shields its jumps
    void visit(CDoWhile &) override {}
    void visit(CFor &) override {}
    void visit(CSwitch &node) override {
        // A switch shields break but not continue.
        if (findContinueOnly) {
            node.body().accept(*this);
            return;
        }
        FindsLoopJump inner;
        inner.findContinueOnly = true;
        node.body().accept(inner);
        if (inner.found) found = true;
    }
    void visit(CCompound &node) override {
        std::vector<CStmtPtr> &body = node.body();
        for (std::size_t i = 0; i < body.size(); ++i) body[i]->accept(*this);
    }
    void visit(CIf &node) override {
        node.thenArm().accept(*this);
        if (node.elseArm() != nullptr) node.elseArm()->accept(*this);
    }
    void visit(CCase &node) override { node.body().accept(*this); }
    void visit(CLabel &node) override { node.body().accept(*this); }
    void visit(CExprStmt &) override {}
    void visit(CEmpty &) override {}
    void visit(CReturn &) override {}
    void visit(CGoto &) override {}
    void visit(CDeclStmt &) override {}
    void visit(CBeyond &) override {}
    void visit(CIntLit &) override {}
    void visit(CFloatLit &) override {}
    void visit(CCharLit &) override {}
    void visit(CStringLit &) override {}
    void visit(CIdent &) override {}
    void visit(CUnary &) override {}
    void visit(CBinary &) override {}
    void visit(CAssign &) override {}
    void visit(CTernary &) override {}
    void visit(CCall &) override {}
    void visit(CIndex &) override {}
    void visit(CMember &) override {}
    void visit(CCast &) override {}
    void visit(CSizeof &) override {}
    void visit(CComma &) override {}
};

bool containsLoopJump(CStmt &node, bool continueOnly) {
    FindsLoopJump finder;
    finder.findContinueOnly = continueOnly;
    node.accept(finder);
    return finder.found;
}

}  // namespace

void CToS::visit(CDoWhile &node) {
    // Peeled: the body once, then the while. A break or continue in the
    // peeled copy would sit outside any loop - a Shalimar parse error - so
    // that shape is a marker instead.
    if (containsLoopJump(node.body(), false)) {
        markBeyond(node.offset(),
                   "a do-while whose body breaks or continues - the peeled "
                   "first pass would put them outside a loop");
        return;
    }
    block(node.body(), block_);
    shalimar::ExprPtr cond = expression(node.cond());
    shalimar::Block body;
    ++loopDepth_;
    block(node.body(), &body);
    --loopDepth_;
    block_->push_back(shalimar::StmtPtr(new shalimar::While(
        std::move(cond), std::move(body), lineOf(node.offset()))));
}

bool CToS::counterEscapes(CFor &node, const std::string &name) const {
    // Shalimar's 'for i : a to b' binds its own counter for the duration of
    // the loop. C's does not: after a C for, the variable still holds
    // whatever ended the loop - the limit it failed, or the value it broke
    // at - and reading it there is how you find an index.
    //
    // So the counting form is only faithful when nothing reads the counter
    // after the loop. 'After' is decided on source position: anything
    // reaching past the last offset inside the loop is later than it. A read
    // before the loop cannot see the stale value and does not count, which
    // matters - two 'for' loops over the same 'i' are ordinary, and treating
    // the second's init as an escape would cost the first its counting form
    // for nothing.
    if (currentFn_ == nullptr) return true;   // nothing to look at; be safe
    NameScan loop(name);
    loop.run(node);
    NameScan whole(name);
    whole.run(currentFn_->body());
    const std::vector<std::size_t> &uses = whole.uses();
    for (std::size_t i = 0; i < uses.size(); ++i) {
        if (uses[i] > loop.reach()) return true;
    }
    return false;
}

bool CToS::lowerCountingFor(CFor &node, std::string *escapedCounter) {
    // The counting shape: 'for (i = A; i <= B; i += K)' and its variants,
    // where i is a plain int name. Becomes 'for i : A to B [step K]'.
    // '<' tightens the bound by one; a negative K arrives via 'i -= K' or
    // 'i--'. Anything else returns false and the general lowering runs.
    CExprStmt *initStmt = dynamic_cast<CExprStmt *>(node.init());
    if (initStmt == nullptr || node.cond() == nullptr || node.step() == nullptr) {
        return false;
    }
    CAssign *init = dynamic_cast<CAssign *>(&initStmt->expr());
    if (init == nullptr || init->op() != "=") return false;
    CIdent *counter = dynamic_cast<CIdent *>(&init->target());
    if (counter == nullptr) return false;
    const std::string &name = counter->name();

    CBinary *cond = dynamic_cast<CBinary *>(node.cond());
    if (cond == nullptr) return false;
    CIdent *condVar = dynamic_cast<CIdent *>(&cond->lhs());
    if (condVar == nullptr || condVar->name() != name) return false;
    const std::string &relation = cond->op();
    if (relation != "<" && relation != "<=" && relation != ">" && relation != ">=") {
        return false;
    }

    // The step: i++, ++i, i--, --i, i += K, i -= K.
    long long sign = 0;
    CExpr *stepAmount = nullptr;
    if (CUnary *bump = dynamic_cast<CUnary *>(node.step())) {
        CIdent *stepVar = dynamic_cast<CIdent *>(&bump->operand());
        if (stepVar == nullptr || stepVar->name() != name) return false;
        if (bump->op() == "++") sign = 1;
        else if (bump->op() == "--") sign = -1;
        else return false;
    } else if (CAssign *bump = dynamic_cast<CAssign *>(node.step())) {
        CIdent *stepVar = dynamic_cast<CIdent *>(&bump->target());
        if (stepVar == nullptr || stepVar->name() != name) return false;
        if (bump->op() == "+=") sign = 1;
        else if (bump->op() == "-=") sign = -1;
        else return false;
        stepAmount = &bump->value();
    } else {
        return false;
    }

    // The direction and the relation must agree.
    if ((sign > 0 && (relation == ">" || relation == ">=")) ||
        (sign < 0 && (relation == "<" || relation == "<="))) {
        return false;
    }

    // The shape fits. Whether the form does is a separate question.
    if (counterEscapes(node, name)) {
        if (escapedCounter != nullptr) *escapedCounter = name;
        return false;
    }

    const Info *info = lookup(name);
    const std::string sName = info != nullptr ? info->sName : name;

    shalimar::ExprPtr start = expression(init->value());
    shalimar::ExprPtr end = expression(cond->rhs());
    if (relation == "<") {
        end.reset(new shalimar::Binary(shalimar::Binary::Op::Subtract,
                                       std::move(end), sInt(1)));
    } else if (relation == ">") {
        end.reset(new shalimar::Binary(shalimar::Binary::Op::Add,
                                       std::move(end), sInt(1)));
    }

    shalimar::ExprPtr step;
    if (stepAmount != nullptr) {
        step = expression(*stepAmount);
        if (sign < 0) {
            step.reset(new shalimar::Binary(shalimar::Binary::Op::Subtract,
                                            sInt(0), std::move(step)));
        }
    } else if (sign < 0) {
        step = sInt(-1);
    }
    // sign > 0 with no amount: step 1, Shalimar's default; leave it null.

    shalimar::Block body;
    ++loopDepth_;
    block(node.body(), &body);
    --loopDepth_;

    block_->push_back(shalimar::StmtPtr(new shalimar::For(
        sName, std::move(start), std::move(end), std::move(step),
        std::move(body), lineOf(node.offset()))));
    return true;
}

void CToS::visit(CFor &node) {
    std::string escaped;
    if (lowerCountingFor(node, &escaped)) return;

    // The general lowering: init; while (cond) { body; step }. A continue
    // in the body would skip the step - the shape C defines around - so
    // that combination is a marker.
    if (containsLoopJump(node.body(), true)) {
        markBeyond(node.offset(),
                   escaped.empty()
                       ? std::string(
                             "a for loop that does not count and whose body "
                             "continues - the lowered while would skip the step")
                       : "a for loop whose counter '" + escaped + "' is read "
                         "after the loop and whose body continues - reading it "
                         "afterwards rules out Shalimar's counting for, which "
                         "binds its own counter, and the while left to lower "
                         "to would skip its step at the continue");
        return;
    }

    if (node.init() != nullptr) statement(*node.init());

    shalimar::ExprPtr cond;
    if (node.cond() != nullptr) {
        cond = expression(*node.cond());
    } else {
        cond = sInt(1);
    }

    shalimar::Block body;
    ++loopDepth_;
    block(node.body(), &body);
    if (node.step() != nullptr) {
        shalimar::Block *saved = block_;
        block_ = &body;
        CExpr &step = *node.step();
        if (CUnary *bump = dynamic_cast<CUnary *>(&step)) {
            if (bump->op() == "++" || bump->op() == "--") {
                block_->push_back(shalimar::StmtPtr(new shalimar::CompoundAssign(
                    expression(bump->operand()), bump->op() == "++", sInt(1),
                    lineOf(node.offset()))));
            } else {
                markBeyond(node.offset(), "a for step this converter cannot carry");
            }
        } else if (CAssign *assign = dynamic_cast<CAssign *>(&step)) {
            if (assign->op() == "+=" || assign->op() == "-=") {
                block_->push_back(shalimar::StmtPtr(new shalimar::CompoundAssign(
                    expression(assign->target()), assign->op() == "+=",
                    expression(assign->value()), lineOf(node.offset()))));
            } else if (assign->op() == "=") {
                block_->push_back(shalimar::StmtPtr(new shalimar::Assign(
                    expression(assign->target()), expression(assign->value()),
                    lineOf(node.offset()))));
            } else {
                markBeyond(node.offset(), "a for step this converter cannot carry");
            }
        } else {
            markBeyond(node.offset(), "a for step this converter cannot carry");
        }
        block_ = saved;
    }
    --loopDepth_;

    block_->push_back(shalimar::StmtPtr(new shalimar::While(
        std::move(cond), std::move(body), lineOf(node.offset()))));
}

void CToS::lowerSwitch(CSwitch &node) {
    // The selector is saved once - C evaluates it once - into a temporary
    // the hoist walk declared at the top of the function, then the cases
    // become an if / elseif / else chain testing it. Grouped empty cases join with '|'; each case's trailing break
    // drops; genuine fall-through, or a break bound to the switch from
    // inside an if, is a marker.
    CCompound *body = dynamic_cast<CCompound *>(&node.body());
    if (body == nullptr) {
        markBeyond(node.offset(), "a switch whose body is not a block");
        return;
    }

    // The selector's name and its Declare came from the hoist walk; only
    // the assignment belongs here, where the switch stands.
    std::map<std::size_t, std::string>::const_iterator minted =
        switchTemps_.find(node.offset());
    if (minted == switchTemps_.end()) {
        markBeyond(node.offset(), "a switch the hoist walk never reached");
        return;
    }
    const std::string selector = minted->second;

    block_->push_back(shalimar::StmtPtr(new shalimar::Assign(
        shalimar::ExprPtr(new shalimar::Var(selector)), expression(node.cond()),
        lineOf(node.offset()))));

    // Walk the case list. Each entry in the C tree is a CCase owning its
    // labelled statement; the rest of a case's body is the following
    // statements up to the next CCase.
    struct Arm {
        std::vector<shalimar::ExprPtr> values;   // empty for default
        bool isDefault = false;
        std::vector<CStmt *> body;
        std::size_t offset = 0;
    };
    std::vector<Arm> arms;

    std::vector<CStmtPtr> &items = body->body();
    for (std::size_t i = 0; i < items.size(); ++i) {
        CStmt *item = items[i].get();
        while (CCase *label = dynamic_cast<CCase *>(item)) {
            Arm arm;
            arm.offset = label->offset();
            if (label->isDefault()) {
                arm.isDefault = true;
            } else {
                arm.values.push_back(expression(*label->value()));
            }
            // Grouped labels: the owned statement is itself a case.
            CStmt *inner = &label->body();
            while (CCase *grouped = dynamic_cast<CCase *>(inner)) {
                if (grouped->isDefault()) arm.isDefault = true;
                else arm.values.push_back(expression(*grouped->value()));
                inner = &grouped->body();
            }
            arms.push_back(std::move(arm));
            if (dynamic_cast<CEmpty *>(inner) == nullptr) {
                arms.back().body.push_back(inner);
            }
            item = nullptr;
            break;
        }
        if (item == nullptr) continue;
        if (arms.empty()) {
            markBeyond(node.offset(), "a statement before the first case");
            return;
        }
        arms.back().body.push_back(item);
    }

    // Convert each arm; the trailing break drops, an absent one that is not
    // the last arm is fall-through, and a break elsewhere is C's escape
    // from the switch, which the chain has no need of - but only when it is
    // the tail. Deeper breaks are markers.
    std::unique_ptr<shalimar::If> chain(new shalimar::If(lineOf(node.offset())));
    shalimar::Block defaultBody;
    bool haveDefault = false;
    bool haveBranch = false;

    for (std::size_t i = 0; i < arms.size(); ++i) {
        Arm &arm = arms[i];

        bool endsWithBreak = false;
        if (!arm.body.empty() &&
            dynamic_cast<CBreak *>(arm.body.back()) != nullptr) {
            endsWithBreak = true;
            arm.body.pop_back();
        }
        if (!arm.body.empty() &&
            dynamic_cast<CReturn *>(arm.body.back()) != nullptr) {
            endsWithBreak = true;    // a return ends the case as surely
        }
        if (!endsWithBreak && i + 1 < arms.size()) {
            markBeyond(arm.offset,
                       "a case that falls through into the next - materialise "
                       "it by hand");
            continue;
        }

        shalimar::Block armBody;
        shalimar::Block *saved = block_;
        block_ = &armBody;
        for (std::size_t k = 0; k < arm.body.size(); ++k) {
            if (dynamic_cast<CBreak *>(arm.body[k]) != nullptr) {
                markBeyond(arm.body[k]->offset(),
                           "a break inside the case but not at its end");
                continue;
            }
            statement(*arm.body[k]);
        }
        block_ = saved;

        if (arm.isDefault) {
            haveDefault = true;
            defaultBody = std::move(armBody);
            continue;
        }

        // The condition: selector = v1 | selector = v2 | ...
        shalimar::ExprPtr cond;
        for (std::size_t v = 0; v < arm.values.size(); ++v) {
            shalimar::ExprPtr test(new shalimar::Binary(
                shalimar::Binary::Op::Equal,
                shalimar::ExprPtr(new shalimar::Var(selector)),
                std::move(arm.values[v])));
            if (cond == nullptr) cond = std::move(test);
            else cond.reset(new shalimar::Binary(shalimar::Binary::Op::Or,
                                                 std::move(cond), std::move(test)));
        }
        chain->addBranch(std::move(cond), std::move(armBody));
        haveBranch = true;
    }

    if (haveDefault) chain->setElse(std::move(defaultBody));
    if (haveBranch) {
        block_->push_back(shalimar::StmtPtr(chain.release()));
    }
}

void CToS::visit(CSwitch &node) {
    lowerSwitch(node);
}

void CToS::visit(CCase &node) {
    // Reached only outside a switch body walk, which lowerSwitch prevents.
    markBeyond(node.offset(), "a case label outside a switch this converter read");
}

void CToS::visit(CBreak &node) {
    if (loopDepth_ == 0) {
        markBeyond(node.offset(), "a break bound to a switch, not a loop");
        return;
    }
    block_->push_back(shalimar::StmtPtr(
        new shalimar::Break(lineOf(node.offset()))));
}

void CToS::visit(CContinue &node) {
    block_->push_back(shalimar::StmtPtr(
        new shalimar::Continue(lineOf(node.offset()))));
}

bool CToS::lowerTernaryReturn(CTernary &top, std::size_t offset,
                             shalimar::Block *into) {
    // 'return c ? a : b' as an if / else with a return in each arm. An
    // assignment's version of this writes the target twice; there is no
    // target here, but two returns say the same thing.
    const int before = beyondCount_;
    std::unique_ptr<shalimar::If> branch(new shalimar::If(lineOf(offset)));

    CTernary *walk = &top;
    for (;;) {
        shalimar::ExprPtr cond = expression(walk->cond());
        shalimar::Block thenBody;
        if (!returnArm(walk->thenArm(), offset, &thenBody)) return false;
        branch->addBranch(std::move(cond), std::move(thenBody));

        // A conditional in the else position flattens into 'elseif' rather
        // than nesting: 'elseif' is one keyword in Shalimar and an 'if'
        // inside an 'else' is not grammatical. visit(CIf) walks its chain
        // for the same reason.
        if (CTernary *chained = dynamic_cast<CTernary *>(&walk->elseArm())) {
            walk = chained;
            continue;
        }
        shalimar::Block elseBody;
        if (!returnArm(walk->elseArm(), offset, &elseBody)) return false;
        branch->setElse(std::move(elseBody));
        break;
    }

    // Something in an arm was refused. The marker is already in the block
    // where the return stood; adding a branch that returns a placeholder
    // beside it would be a function quietly answering the wrong number.
    if (beyondCount_ != before) return false;
    into->push_back(shalimar::StmtPtr(branch.release()));
    return true;
}

bool CToS::returnArm(CExpr &value, std::size_t offset, shalimar::Block *into) {
    // A conditional in the 'then' position nests instead, which is
    // grammatical - it is only 'else' that cannot take an 'if'.
    if (CTernary *nested = dynamic_cast<CTernary *>(&value)) {
        return lowerTernaryReturn(*nested, offset, into);
    }
    std::unique_ptr<shalimar::Return> ret(new shalimar::Return(lineOf(offset)));
    ret->add(expression(value));
    into->push_back(shalimar::StmtPtr(ret.release()));
    return true;
}

void CToS::visit(CReturn &node) {
    // 'return c ? a : b' expands into statements, so it is taken before the
    // value is treated as an expression. Not in main, where a returned
    // value is an exit status and has no Shalimar meaning either way.
    if (node.value() != nullptr && !currentIsMain_) {
        if (CTernary *ternary = dynamic_cast<CTernary *>(node.value())) {
            lowerTernaryReturn(*ternary, node.offset(), block_);
            return;
        }
    }

    const int before = beyondCount_;
    std::unique_ptr<shalimar::Return> ret(
        new shalimar::Return(lineOf(node.offset())));
    if (node.value() != nullptr) {
        // main's exit status has no Shalimar meaning; 'return 0' drops its
        // value, anything else is worth a marker.
        if (currentIsMain_) {
            CIntLit *lit = dynamic_cast<CIntLit *>(node.value());
            if (lit == nullptr || lit->value() != 0) {
                markBeyond(node.offset(),
                           "main returning a status - Shalimar has none");
            }
        } else {
            ret->add(expression(*node.value()));
        }
    }
    if (beyondCount_ != before) {
        // The value could not be carried; a bare return here would silently
        // change what the function answers, so the marker stands alone.
        return;
    }
    block_->push_back(shalimar::StmtPtr(ret.release()));
}

void CToS::visit(CGoto &node) {
    markBeyond(node.offset(), "'goto' - Shalimar has no labels");
}

void CToS::visit(CLabel &node) {
    markBeyond(node.offset(), "a label - Shalimar has no goto");
    statement(node.body());
}

void CToS::visit(CCompound &node) {
    // A bare block is not a construct in Shalimar; its statements flatten
    // into the enclosing body, with their own name scope.
    block(node, block_);
}

// ------------------------------------------------------------------- print

void CToS::lowerPrintf(CCall &call) {
    CIdent *callee = static_cast<CIdent *>(&call.callee());
    const std::string &name = callee->name();
    const int line = lineOf(call.offset());

    if (name == "putchar") {
        if (call.args().size() != 1) {
            markBeyond(call.offset(), "putchar without its one argument");
            return;
        }
        CCharLit *lit = dynamic_cast<CCharLit *>(call.args()[0].get());
        if (lit != nullptr && lit->value() == '\n') {
            block_->push_back(shalimar::StmtPtr(new shalimar::Print(true, line)));
            return;
        }
        std::unique_ptr<shalimar::Print> print(new shalimar::Print(false, line));
        print->add(charWrap(expression(*call.args()[0])));
        block_->push_back(shalimar::StmtPtr(print.release()));
        return;
    }

    if (name == "puts") {
        if (call.args().size() != 1) {
            markBeyond(call.offset(), "puts without its one argument");
            return;
        }
        std::unique_ptr<shalimar::Print> print(new shalimar::Print(true, line));
        print->add(expression(*call.args()[0]));
        block_->push_back(shalimar::StmtPtr(print.release()));
        return;
    }

    // printf. The format must be a literal; each %-hole consumes one
    // argument; embedded newlines split into one print per line. Width,
    // precision and the length modifiers are beyond this lowering.
    std::vector<CExprPtr> &args = call.args();
    if (args.empty()) {
        markBeyond(call.offset(), "printf with no format");
        return;
    }
    CStringLit *format = dynamic_cast<CStringLit *>(args[0].get());
    if (format == nullptr) {
        markBeyond(call.offset(), "printf whose format is not a literal");
        return;
    }

    const std::string &text = format->text();
    std::size_t nextArg = 1;
    std::unique_ptr<shalimar::Print> print(new shalimar::Print(false, line));
    bool printHasItems = false;
    std::string pending;

    // Note what the item spacing does: Shalimar writes one space after
    // every item, so the converted program's output matches printf's up to
    // whitespace, not byte for byte. That is the cost of '?', and the
    // honest place to say so is here.
    struct Flush {
        void operator()(std::unique_ptr<shalimar::Print> &print,
                        std::string &pending, bool &printHasItems) const {
            if (pending.empty()) return;
            print->add(shalimar::ExprPtr(new shalimar::StrLit(pending)));
            pending.clear();
            printHasItems = true;
        }
    } flush;

    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '\n') {
            flush(print, pending, printHasItems);
            // This print ends its line.
            std::unique_ptr<shalimar::Print> done(new shalimar::Print(true, line));
            std::vector<shalimar::ExprPtr> &items = print->items();
            for (std::size_t k = 0; k < items.size(); ++k) {
                done->add(std::move(items[k]));
            }
            block_->push_back(shalimar::StmtPtr(done.release()));
            print.reset(new shalimar::Print(false, line));
            printHasItems = false;
            continue;
        }
        if (c != '%') {
            if (static_cast<unsigned char>(c) < 32 || c == '"') {
                markBeyond(call.offset(),
                           "a format character Shalimar cannot spell");
                return;
            }
            // The mirror of the strip before a hole: a space directly after
            // one is the '?' item space already written.
            if (c == ' ' && pending.empty() && printHasItems) continue;
            pending += c;
            continue;
        }
        // A specifier.
        ++i;
        if (i >= text.size()) {
            markBeyond(call.offset(), "a format ending in '%'");
            return;
        }
        const char spec = text[i];
        if (spec == '%') {
            pending += '%';
            continue;
        }
        if (nextArg >= args.size()) {
            markBeyond(call.offset(), "more format holes than arguments");
            return;
        }
        // '?' writes a space after every item, so the single space printf
        // formats usually put around a hole is already provided; dropping
        // it here brings the outputs to within a trailing space of equal.
        if (!pending.empty() && pending[pending.size() - 1] == ' ') {
            pending.erase(pending.size() - 1);
        }
        flush(print, pending, printHasItems);
        shalimar::ExprPtr item = expression(*args[nextArg]);
        if (spec == 'f') {
            // printf's %f writes six decimals; Shalimar's default is seven.
            // prec(6) closes the gap, and stays set - every later real in a
            // program lowered here comes from the same %f convention.
            print->add(shalimar::ExprPtr(new shalimar::Precision(sInt(6))));
            print->add(std::move(item));
        } else if (spec == 'd' || spec == 'i' || spec == 'g' ||
                   spec == 'e' || spec == 's') {
            // '%d' of a char prints its code in C, and '?' of a char prints
            // the character. Same promotion as the arithmetic one above.
            if ((spec == 'd' || spec == 'i') && isCharContext(*args[nextArg])) {
                item = intWrap(std::move(item));
            }
            print->add(std::move(item));
        } else if (spec == 'c') {
            print->add(charWrap(std::move(item)));
        } else {
            markBeyond(call.offset(),
                       std::string("the '%") + spec + "' format - only plain "
                       "%d %i %f %g %e %s %c carry");
            return;
        }
        ++nextArg;
        printHasItems = true;
    }

    flush(print, pending, printHasItems);
    if (printHasItems) {
        // No trailing newline: '??'.
        block_->push_back(shalimar::StmtPtr(print.release()));
    }
}

// ------------------------------------------------------------ declarations

void CToS::declareLocal(CDeclaration &decl, bool atTop) {
    // Emits the Shalimar Declare - always at the top of whatever block_ is,
    // which for the hoist walk is the top of the function - and renames the
    // C name where it collides with one already handed out.
    //
    // Where the Info is then registered is what 'atTop' decides. The hoist
    // (atTop) parks it in hoisted_ for the statement walk to bind in the
    // right scope; a global (not atTop) goes straight into the outermost
    // scope, which is the only one it could be in.

    std::vector<CDeclaration::Declarator> &declarators = decl.declarators();
    for (std::size_t i = 0; i < declarators.size(); ++i) {
        CDeclaration::Declarator &declarator = declarators[i];
        CType *type = declarator.type.get();

        // Count array layers down to the scalar.
        int rank = 0;
        CType *walk = type;
        std::vector<CNode *> extents;
        while (walk != nullptr && walk->kind() == CType::Kind::Array) {
            ++rank;
            extents.push_back(walk->length());
            walk = walk->base();
        }

        if (walk == nullptr ||
            (walk->kind() != CType::Kind::Int && walk->kind() != CType::Kind::Char &&
             walk->kind() != CType::Kind::Float && walk->kind() != CType::Kind::Double)) {
            markBeyond(declarator.offset,
                       "the declaration of '" + declarator.name + "' - " +
                       (type != nullptr ? type->describe() : std::string("?")) +
                       " has no Shalimar type");
            continue;
        }
        bool lossy = false;
        const shalimar::Type *scalar = scalarS(*walk, &lossy);
        if (scalar == nullptr || lossy) {
            markBeyond(declarator.offset,
                       "the declaration of '" + declarator.name + "' - " +
                       walk->describe() + " has no Shalimar type");
            continue;
        }
        if (scalar->kind() == shalimar::Type::Kind::Char && rank > 1) {
            markBeyond(declarator.offset,
                       "'" + declarator.name + "': char arrays above rank one - "
                       "Shalimar strings are 1-D");
            continue;
        }

        Info info;
        info.sName = rename(declarator.name);
        info.rank = rank;
        info.isChar = scalar->kind() == shalimar::Type::Kind::Char;
        info.type = scalar;
        if (atTop) {
            // The hoist has no scope stack to register into; it parks the
            // Info and the CDeclStmt visit binds it in the right scope.
            hoisted_[declarator.offset] = info;
        } else {
            scopes_.back()[declarator.name] = info;
        }

        std::unique_ptr<shalimar::Declare> made(new shalimar::Declare(
            scalar, info.sName, nullptr, lineOf(declarator.offset)));
        bool extentsOk = true;
        for (std::size_t k = 0; k < extents.size(); ++k) {
            if (extents[k] == nullptr) {
                markBeyond(declarator.offset,
                           "'" + declarator.name + "': an array without a bound");
                extentsOk = false;
                break;
            }
            made->addExtent(expression(static_cast<CExpr &>(*extents[k])));
        }
        if (!extentsOk) continue;

        // A constant initialiser rides the hoisted Declare; a scalar's
        // runtime initialiser runs where the declaration stood (the
        // CDeclStmt visit emits it).
        if (declarator.init != nullptr) {
            if (declarator.init->isList()) {
                std::unique_ptr<shalimar::ArrayLit> literal(new shalimar::ArrayLit());
                struct Lift {
                    CToS *self;
                    shalimar::ExprPtr run(CInit &init) {
                        if (!init.isList()) {
                            return self->expression(*init.expr());
                        }
                        std::unique_ptr<shalimar::ArrayLit> inner(
                            new shalimar::ArrayLit());
                        std::vector<CInit> &items = init.items();
                        for (std::size_t k = 0; k < items.size(); ++k) {
                            inner->add(run(items[k]));
                        }
                        return shalimar::ExprPtr(inner.release());
                    }
                } lift{this};
                std::vector<CInit> &items = declarator.init->items();
                for (std::size_t k = 0; k < items.size(); ++k) {
                    literal->add(lift.run(items[k]));
                }
                made->initial() = shalimar::ExprPtr(literal.release());
            } else if (rank > 0) {
                // 'char s[32] = "hello"' - the string rides the Declare.
                if (CStringLit *stringInit =
                        dynamic_cast<CStringLit *>(declarator.init->expr())) {
                    made->initial() =
                        shalimar::ExprPtr(new shalimar::StrLit(stringInit->text()));
                } else {
                    markBeyond(declarator.offset,
                               "'" + declarator.name +
                               "': an array initialised from an expression");
                }
            } else if (!atTop) {
                // A global scalar. It has no CDeclStmt site to run an
                // initialiser at - convertTopDeclaration never visits one -
                // so the value has to ride the Declare or be lost, which is
                // what used to happen: 'double scale = 1.5;' became a plain
                // 'real scale' and every use of it read zero. C89 requires a
                // file-scope initialiser to be a constant expression, so
                // there is nothing here that needed a statement anyway.
                shalimar::ExprPtr value = expression(*declarator.init->expr());
                if (dynamic_cast<CCharLit *>(declarator.init->expr()) != nullptr &&
                    info.isChar) {
                    value = charWrap(std::move(value));
                }
                made->initial() = std::move(value);
            }
            // A local scalar's initialiser runs at its CDeclStmt site, in
            // the block where it stood.
        }

        block_->push_back(shalimar::StmtPtr(made.release()));
    }
}

void CToS::hoistDeclarations(CStmt &node, shalimar::Block *top) {
    // Every declaration in the function, at any depth, becomes a top-level
    // Declare - Shalimar refuses one anywhere else. A genuine C shadow at an
    // inner scope arrives here as a second declaration of the same name and
    // gets a fresh name from rename()'s collision rule; which of the two any
    // given reference means is settled later, by the statement walk, since
    // this walk has no scope stack to say.
    if (CDeclStmt *decl = dynamic_cast<CDeclStmt *>(&node)) {
        shalimar::Block *saved = block_;
        block_ = top;
        // A local 'static' outlives the call and is initialised once.
        // Hoisted like any other local it becomes a plain variable that is
        // initialised again on every call - which compiles, runs, and
        // quietly means something else. Shalimar has no per-function
        // persistence to put it in, so it is refused here, where a
        // declaration is a local by construction.
        //
        // 'register' and 'auto' pass through: in C89 they are hints with no
        // effect on meaning. A file-scope 'static' passes through too, in
        // convertTopDeclaration - it says 'not visible to other translation
        // units', and a Shalimar program is whole, so there are none.
        if (decl->decl().storage() == CDeclaration::Storage::Static) {
            std::vector<CDeclaration::Declarator> &statics =
                decl->decl().declarators();
            for (std::size_t i = 0; i < statics.size(); ++i) {
                markBeyond(statics[i].offset,
                           "'" + statics[i].name + "' declared static inside "
                           "a function - it would keep its value between "
                           "calls, and Shalimar has nowhere to keep it");
            }
        } else {
            declareLocal(decl->decl(), true);
        }
        block_ = saved;
        return;
    }
    if (CCompound *compound = dynamic_cast<CCompound *>(&node)) {
        std::vector<CStmtPtr> &body = compound->body();
        for (std::size_t i = 0; i < body.size(); ++i) {
            hoistDeclarations(*body[i], top);
        }
        return;
    }
    if (CIf *branch = dynamic_cast<CIf *>(&node)) {
        hoistDeclarations(branch->thenArm(), top);
        if (branch->elseArm() != nullptr) hoistDeclarations(*branch->elseArm(), top);
        return;
    }
    if (CWhile *loop = dynamic_cast<CWhile *>(&node)) {
        hoistDeclarations(loop->body(), top);
        return;
    }
    if (CDoWhile *loop = dynamic_cast<CDoWhile *>(&node)) {
        hoistDeclarations(loop->body(), top);
        return;
    }
    if (CFor *loop = dynamic_cast<CFor *>(&node)) {
        if (loop->init() != nullptr) hoistDeclarations(*loop->init(), top);
        hoistDeclarations(loop->body(), top);
        return;
    }
    if (CSwitch *sw = dynamic_cast<CSwitch *>(&node)) {
        // The selector temporary is a declaration like any other, and
        // Shalimar wants it at the top of the function - so it is minted
        // here, where 'the top' is still reachable, rather than in
        // lowerSwitch, which runs during the statement walk and could only
        // put it wherever the switch happened to be. A switch inside a loop
        // declared it inside the loop, and shc refused the file.
        char temp[24];
        std::snprintf(temp, sizeof temp, "sw_%d", ++tempCount_);
        const std::string selector = rename(temp);
        switchTemps_[sw->offset()] = selector;
        top->push_back(shalimar::StmtPtr(new shalimar::Declare(
            shalimar::Type::intType(), selector, nullptr,
            lineOf(sw->offset()))));
        hoistDeclarations(sw->body(), top);
        return;
    }
    if (CCase *label = dynamic_cast<CCase *>(&node)) {
        hoistDeclarations(label->body(), top);
        return;
    }
    if (CLabel *label = dynamic_cast<CLabel *>(&node)) {
        hoistDeclarations(label->body(), top);
        return;
    }
}

// ----------------------------------------------------------------- program

void CToS::convertFunction(CFunctionDef &fn) {
    const CType &type = fn.type();
    currentIsMain_ = fn.name() == "main";

    const Info *registered = lookup(fn.name());
    shalimar::Prototype proto(
        registered != nullptr ? registered->sName
                              : (currentIsMain_ ? std::string("main")
                                                : rename(fn.name())),
        lineOf(fn.offset()));

    // Outputs: void is none; int, double, float, char are one; main's int
    // becomes none - its status has no Shalimar meaning.
    const CType *returns = type.base();
    if (returns != nullptr && returns->kind() != CType::Kind::Void &&
        !currentIsMain_) {
        bool lossy = false;
        const shalimar::Type *scalar = scalarS(*returns, &lossy);
        if (scalar == nullptr || lossy) {
            markBeyond(fn.offset(),
                       "the whole function '" + fn.name() + "' - it returns " +
                       returns->describe());
            // Emit the marker at global level by holding it in a block that
            // convert() routes; simplest is a function-shaped marker:
            return;
        }
        proto.outputs.push_back(scalar);
    }

    scopes_.push_back(std::map<std::string, Info>());
    currentFn_ = &fn;

    // Parameters: scalars by value, arrays as themselves, pointers to
    // scalars as '&name' references when every use in the body is '*name' -
    // this milestone takes the simpler road and marks pointer parameters.
    bool signatureOk = true;
    const std::vector<CType::Param> &params = type.params();
    for (std::size_t i = 0; i < params.size(); ++i) {
        const CType::Param &param = params[i];
        const CType *ptype = param.type.get();

        int rank = 0;
        const CType *walk = ptype;
        while (walk != nullptr && walk->kind() == CType::Kind::Array) {
            // The source-faithful tree keeps 'T a[]' as an Array; only a
            // written '*' is a Pointer.
            ++rank;
            walk = walk->base();
        }
        if (walk != nullptr && walk->kind() == CType::Kind::Pointer) {
            markBeyond(fn.offset(),
                       "parameter '" + param.name + "' of '" + fn.name() +
                       "' is a pointer");
            signatureOk = false;
            continue;
        }
        bool lossy = false;
        const shalimar::Type *scalar =
            walk != nullptr ? scalarS(*walk, &lossy) : nullptr;
        if (scalar == nullptr || lossy) {
            markBeyond(fn.offset(),
                       "parameter '" + param.name + "' of '" + fn.name() + "' - " +
                       (ptype != nullptr ? ptype->describe() : std::string("?")));
            signatureOk = false;
            continue;
        }

        Info info;
        info.sName = rename(param.name);
        info.rank = rank;
        info.isChar = scalar->kind() == shalimar::Type::Kind::Char;
        info.type = scalar;
        scopes_.back()[param.name] = info;

        shalimar::Param sParam;
        sParam.name = info.sName;
        const shalimar::Type *full = scalar;
        for (int r = 0; r < rank; ++r) full = shalimar::Type::arrayOf(full);
        sParam.type = full;
        sParam.byReference = false;
        proto.inputs.push_back(sParam);
    }

    shalimar::Block body;
    if (signatureOk) {
        shalimar::Block *saved = block_;
        block_ = &body;
        hoistDeclarations(fn.body(), &body);
        block_ = saved;
        block(fn.body(), &body);
    } else {
        // The signature already spoke; the body would only cascade.
    }

    scopes_.pop_back();

    std::unique_ptr<shalimar::Function> made(
        new shalimar::Function(std::move(proto), std::move(body)));
    program_->add(std::move(made));
    currentIsMain_ = false;
    currentFn_ = nullptr;
}

std::unique_ptr<shalimar::Program> CToS::convert(CProgram &program) {
    program_.reset(new shalimar::Program());
    scopes_.clear();
    scopes_.push_back(std::map<std::string, Info>());

    // Pass one: learn every function defined here, so calls resolve, and
    // give each its Shalimar name.
    std::vector<std::unique_ptr<CFunctionDef>> &functions = program.functions();
    for (std::size_t i = 0; i < functions.size(); ++i) {
        knownFunctions_.insert(functions[i]->name());
        Info info;
        info.sName = functions[i]->name() == "main"
                         ? "main" : rename(functions[i]->name());
        scopes_.back()[functions[i]->name()] = info;
        usedNames_.insert(info.sName);
    }

    const std::vector<CProgram::Entry> &order = program.order();
    for (std::size_t i = 0; i < order.size(); ++i) {
        const CProgram::Entry &entry = order[i];
        if (entry.isFunction) {
            convertFunction(*functions[entry.index]);
        } else {
            CDeclaration &decl = *program.declarations()[entry.index];
            convertTopDeclaration(decl);
        }
    }

    return std::move(program_);
}

void CToS::convertTopDeclaration(CDeclaration &decl) {
    // Prototypes drop silently - Shalimar needs none. Typedefs, structs,
    // enums and externs are markers; scalar and array globals carry.
    if (decl.storage() == CDeclaration::Storage::Typedef) {
        markBeyond(declOffset(decl), "a typedef - Shalimar has no named types");
        return;
    }
    if (decl.bareType() != nullptr) {
        markBeyond(declOffset(decl),
                   decl.bareType()->describe() + " - Shalimar has no such type");
        return;
    }
    if (decl.storage() == CDeclaration::Storage::Extern) {
        markBeyond(declOffset(decl),
                   "an extern declaration - a Shalimar program is whole");
        return;
    }

    std::vector<CDeclaration::Declarator> &declarators = decl.declarators();
    bool anyPrototypes = false;
    for (std::size_t i = 0; i < declarators.size(); ++i) {
        if (declarators[i].type != nullptr &&
            declarators[i].type->kind() == CType::Kind::Function) {
            anyPrototypes = true;
        }
    }
    if (anyPrototypes) return;      // dropped: functions are collected whole

    // A global marker has no block to land in; globals go through a
    // holding block, then into the program.
    shalimar::Block holder;
    shalimar::Block *saved = block_;
    block_ = &holder;
    declareLocal(decl, false);
    block_ = saved;
    for (std::size_t i = 0; i < holder.size(); ++i) {
        // Declares and markers both, in the order they were made. A marker
        // dropped here was the third way a refusal could vanish: the count
        // still rose, so the run reported a construct it had nowhere shown,
        // and the output was a smaller program that compiled.
        program_->addGlobal(std::move(holder[i]));
    }
}

std::size_t CToS::declOffset(CDeclaration &decl) const {
    if (!decl.declarators().empty()) return decl.declarators()[0].offset;
    return decl.offset();
}

}  // namespace c2s
