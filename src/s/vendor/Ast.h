// The syntax tree.
//
// This grows one language feature at a time. Every node here is reachable
// from a program that compiles and runs on all three targets; a node is not
// added until the feature it carries is being finished.
//
// Nodes are polymorphic and are walked by double dispatch. A pass is a class
// deriving from NodeVisitor - the checker, the code generator - so adding a
// pass costs no change here, and adding a node makes the compiler name every
// pass that has not yet handled it. That is the point of the pure-virtual
// list below: a forgotten pass is a build error rather than a wrong answer at
// run time.
//
// Only statements carry a line. An error inside a long expression names the
// statement containing it, because that is the useful answer and a
// per-expression line would need a call stack to be worth having.
#pragma once

#include "Type.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace shalimar {

class IntLit;
class RealLit;
class Var;
class Convert;
class Binary;
class Declare;
class Assign;
class CompoundAssign;
class Print;
class If;
class While;
class For;
class Break;
class Continue;
class Call;
class Return;
class MultiAssign;
class CallStmt;
class StrLit;
class ArrayLit;
class Blank;
class Index;
class Dim;
class Precision;

class NodeVisitor {
public:
    virtual ~NodeVisitor() = default;

    // Non-const, because a pass may annotate what it walks: the checker
    // records the type it worked out on the node itself, so the code
    // generator can read it back rather than deciding it a second time. A
    // pass that only reads simply does not write.
    virtual void visit(IntLit &) = 0;
    virtual void visit(RealLit &) = 0;
    virtual void visit(Var &) = 0;
    virtual void visit(Convert &) = 0;
    virtual void visit(Binary &) = 0;
    virtual void visit(Declare &) = 0;
    virtual void visit(Assign &) = 0;
    virtual void visit(CompoundAssign &) = 0;
    virtual void visit(Print &) = 0;
    virtual void visit(If &) = 0;
    virtual void visit(While &) = 0;
    virtual void visit(For &) = 0;
    virtual void visit(Break &) = 0;
    virtual void visit(Continue &) = 0;
    virtual void visit(Call &) = 0;
    virtual void visit(Return &) = 0;
    virtual void visit(MultiAssign &) = 0;
    virtual void visit(CallStmt &) = 0;
    virtual void visit(StrLit &) = 0;
    virtual void visit(ArrayLit &) = 0;
    virtual void visit(Blank &) = 0;
    virtual void visit(Index &) = 0;
    virtual void visit(Dim &) = 0;
    virtual void visit(Precision &) = 0;
};

class Node {
public:
    virtual ~Node() = default;
    virtual void accept(NodeVisitor &v) = 0;

protected:
    Node() = default;
};

// ------------------------------------------------------------------ symbols

// A name, once the checker has resolved it. Storage says where the value
// lives; `slot` is which eight-byte place in the frame holds it.
class Symbol {
public:
    // Where the value lives. A local is a slot in the current frame; a
    // global is a slot in one block the whole program shares, because one
    // symbol and an offset is the same three lines of assembly on every
    // target where a symbol each would be three different ones.
    enum class Storage { Local, Global };

    Symbol(std::string name, const Type *type, int slot, Storage storage = Storage::Local)
        : name_(std::move(name)), type_(type), slot_(slot), storage_(storage) {}

    const std::string &name() const { return name_; }
    const Type *type() const { return type_; }
    int slot() const { return slot_; }
    Storage storage() const { return storage_; }
    bool isGlobal() const { return storage_ == Storage::Global; }

    // A '&' parameter: the slot holds the caller's address rather than the
    // value, so every read and write of the name goes through it.
    bool isReference() const { return reference_; }
    void makeReference() { reference_ = true; }

private:
    std::string name_;
    const Type *type_;
    int slot_;
    Storage storage_;
    bool reference_ = false;
};

// ---------------------------------------------------------------- expressions

class Expr : public Node {
public:
    // Filled in by the checker; null until it has run.
    const Type *type() const { return type_; }
    void setType(const Type *t) { type_ = t; }

    // A reference argument must be addressable - a variable or an element,
    // not a computed value - so 'bump(1+2)' can be refused.
    virtual bool isAddressable() const { return false; }

    // Asked by the parser, which folds a negated literal into the literal
    // rather than emitting a subtraction from zero.
    virtual bool isIntLiteral() const { return false; }
    virtual bool isRealLiteral() const { return false; }

private:
    const Type *type_ = nullptr;
};

using ExprPtr = std::unique_ptr<Expr>;

class IntLit : public Expr {
public:
    explicit IntLit(int32_t value) : value_(value) {}

    int32_t value() const { return value_; }
    bool isIntLiteral() const override { return true; }
    void accept(NodeVisitor &v) override { v.visit(*this); }

private:
    int32_t value_;
};

// A numeral containing a point or an exponent is a real; anything else is an
// int. That is a lexical decision, not a typing one, which is why the two
// literals are different nodes.
class RealLit : public Expr {
public:
    explicit RealLit(double value) : value_(value) {}

    double value() const { return value_; }
    bool isRealLiteral() const override { return true; }
    void accept(NodeVisitor &v) override { v.visit(*this); }

private:
    double value_;
};

class Var : public Expr {
public:
    explicit Var(std::string name) : name_(std::move(name)) {}

    const std::string &name() const { return name_; }
    const Symbol *symbol() const { return symbol_; }
    void resolve(const Symbol *s) { symbol_ = s; }

    // 'pi' and 'e' are values rather than storage, so a Var may turn out to
    // be one. Nothing can write them, which is why one name means one thing
    // here and there is no shadowing to arrange.
    bool isNamedConstant() const { return constant_; }
    double constant() const { return value_; }
    void resolveConstant(double value) { constant_ = true; value_ = value; }

    bool isAddressable() const override { return !constant_; }
    void accept(NodeVisitor &v) override { v.visit(*this); }

private:
    std::string name_;
    const Symbol *symbol_ = nullptr;
    bool constant_ = false;
    double value_ = 0.0;
};

// Inserted by the checker wherever the language converts, which it does in
// both directions and mostly in silence. Nothing else in the tree is
// implicit by the time the code generator sees it: every value has one type,
// and where it changed there is a node saying so.
class Convert : public Expr {
public:
    Convert(ExprPtr expr, const Type *to) : expr_(std::move(expr)) { setType(to); }

    Expr &expr() const { return *expr_; }
    ExprPtr &operand() { return expr_; }
    void accept(NodeVisitor &v) override { v.visit(*this); }

private:
    ExprPtr expr_;
};

// Every operator with two operands, including the comparisons and the two
// logical connectives. Neither '&' nor '|' short-circuits: both sides are
// evaluated before either is asked, which is what makes them ordinary binary
// operators rather than control flow.
//
// Unary minus is not here. It folds into its operand as it is parsed and
// arrives as a literal or as a subtraction from zero, which is why '-2^2' is
// '(-2)^2' and not '-(2^2)': the negation is a finished term before the '^'
// is seen.
class Binary : public Expr {
public:
    enum class Op {
        Add, Subtract, Multiply, Divide, Modulus, Power,
        Equal, NotEqual, Less, Greater, LessEqual, GreaterEqual,
        And, Or
    };

    Binary(Op op, ExprPtr lhs, ExprPtr rhs)
        : op_(op), lhs_(std::move(lhs)), rhs_(std::move(rhs)) {}

    Op op() const { return op_; }
    Expr &lhs() const { return *lhs_; }
    Expr &rhs() const { return *rhs_; }
    ExprPtr &left() { return lhs_; }
    ExprPtr &right() { return rhs_; }

    void accept(NodeVisitor &v) override { v.visit(*this); }

    static const char *spelling(Op op);
    // Whether the result is a truth value rather than a number of the
    // operands' own type.
    static bool yieldsInt(Op op);
    // The runtime entry point that performs it on two operands of a type.
    static const char *runtimeFor(Op op, const Type *operands);

private:
    Op op_;
    ExprPtr lhs_;
    ExprPtr rhs_;
};

// A string literal is the text plus a terminating char(0). It is a char[],
// which makes it the one literal in the language that is an array.
class StrLit : public Expr {
public:
    explicit StrLit(std::string text) : text_(std::move(text)) {}

    const std::string &text() const { return text_; }
    int id() const { return id_; }
    void setId(int id) { id_ = id; }

    void accept(NodeVisitor &v) override { v.visit(*this); }

private:
    std::string text_;
    int id_ = 0;
};

// '{1.0,,}' - a literal whose commas fix its shape. It carries its own
// extents, which is why it is the one right-hand side that may create an
// array: there is nothing left to infer.
class ArrayLit : public Expr {
public:
    void add(ExprPtr element) { elements_.push_back(std::move(element)); }
    std::vector<ExprPtr> &elements() { return elements_; }
    const std::vector<ExprPtr> &elements() const { return elements_; }

    void accept(NodeVisitor &v) override { v.visit(*this); }

private:
    std::vector<ExprPtr> elements_;
};

// An omitted slot: the gap in '{1.0,,}'. It holds the place so the commas
// still describe the shape, and it stands for the zero of the element type.
// A blank carries no type, which is why a literal blank all the way down has
// a shape but nothing to create an array from.
class Blank : public Expr {
public:
    void accept(NodeVisitor &v) override { v.visit(*this); }
};

class Index : public Expr {
public:
    Index(ExprPtr base, ExprPtr index)
        : base_(std::move(base)), index_(std::move(index)) {}

    Expr &base() const { return *base_; }
    Expr &index() const { return *index_; }
    ExprPtr &baseRef() { return base_; }
    ExprPtr &indexRef() { return index_; }

    bool isAddressable() const override { return true; }
    void accept(NodeVisitor &v) override { v.visit(*this); }

private:
    ExprPtr base_;
    ExprPtr index_;
};

// 'A.row' is axis 0, 'A.col' is axis 1, 'A.dim(n)' is axis n. One node for
// all three because they ask the same question - .row and .col are the two
// axes a matrix uses often enough to deserve names, and they work at any
// rank. The spelling is carried only so a diagnostic can quote it back.
class Dim : public Expr {
public:
    Dim(ExprPtr base, ExprPtr axis, std::string spelling)
        : base_(std::move(base)), axis_(std::move(axis)), spelling_(std::move(spelling)) {}

    ExprPtr &base() { return base_; }
    ExprPtr &axis() { return axis_; }
    const std::string &spelling() const { return spelling_; }

    void accept(NodeVisitor &v) override { v.visit(*this); }

private:
    ExprPtr base_;
    ExprPtr axis_;
    std::string spelling_;
};

// '? prec(10)' - a print-list directive, not a value. It prints nothing and
// applies from that point on, including the rest of its own line.
class Precision : public Expr {
public:
    explicit Precision(ExprPtr places) : places_(std::move(places)) {}

    ExprPtr &places() { return places_; }
    void accept(NodeVisitor &v) override { v.visit(*this); }

private:
    ExprPtr places_;
};

struct Prototype;

class Call : public Expr {
public:
    Call(std::string callee, int line) : callee_(std::move(callee)), line_(line) {}

    // A built-in, named by its index in the table in Builtin.cpp. Exactly one
    // of this and prototype() is set once the checker has run.
    int builtin() const { return builtin_; }
    void resolveBuiltin(int index) { builtin_ = index; }

    void add(ExprPtr argument) { arguments_.push_back(std::move(argument)); }

    const std::string &callee() const { return callee_; }
    std::vector<ExprPtr> &arguments() { return arguments_; }
    int line() const { return line_; }

    const Prototype *prototype() const { return prototype_; }
    void resolve(const Prototype *p) { prototype_ = p; }

    // Where the extra outputs of a multi-output call are put, and where a
    // reference argument's copy lives while the call is running. Both are
    // slots the caller lends the callee the address of.
    int scratchBase() const { return scratchBase_; }
    void setScratchBase(int base) { scratchBase_ = base; }

    void accept(NodeVisitor &v) override { v.visit(*this); }

private:
    std::string callee_;
    std::vector<ExprPtr> arguments_;
    int line_;
    const Prototype *prototype_ = nullptr;
    int scratchBase_ = 0;
    int builtin_ = -1;
};

// ----------------------------------------------------------------- statements

class Stmt : public Node {
public:
    int line() const { return line_; }

    // Which source file it came from. Set once, by the parser, from the unit
    // it was parsing - a statement cannot be spread over two files, and a
    // function cannot either.
    int unit() const { return unit_; }
    void setUnit(int unit) { unit_ = unit; }

protected:
    explicit Stmt(int line) : line_(line) {}

private:
    int line_;
    int unit_ = 0;
};

using StmtPtr = std::unique_ptr<Stmt>;
using Block = std::vector<StmtPtr>;

// 'int n : 5'. A declaration may appear only at the top level of a function
// body, or at global scope; the rule keeps every local's lifetime the whole
// call, which is what lets the checker type a function in one pass.
class Declare : public Stmt {
public:
    Declare(const Type *type, std::string name, ExprPtr initial, int line)
        : Stmt(line), type_(type), name_(std::move(name)), initial_(std::move(initial)) {}

    void addExtent(ExprPtr extent) { extents_.push_back(std::move(extent)); }
    std::vector<ExprPtr> &extents() { return extents_; }

    // Where the extents are written down for the runtime to read: one slot
    // each, in order, so that their address can be handed over as an array.
    int extentBase() const { return extentBase_; }
    void setExtentBase(int base) { extentBase_ = base; }

    const Type *declaredType() const { return type_; }
    void setDeclaredType(const Type *type) { type_ = type; }
    const std::string &name() const { return name_; }
    ExprPtr &initial() { return initial_; }        // may be null
    const Symbol *symbol() const { return symbol_; }
    void resolve(const Symbol *s) { symbol_ = s; }

    void accept(NodeVisitor &v) override { v.visit(*this); }

private:
    const Type *type_;
    std::string name_;
    std::vector<ExprPtr> extents_;
    ExprPtr initial_;
    const Symbol *symbol_ = nullptr;
    int extentBase_ = 0;
};

// 'x : expr'. '=' in the same position is a fallback spelling accepted
// silently, and '+:' / '-:' arrive here too, already expanded: 'x +: e' is
// 'x : x + e' and there is nothing left for the code generator to know about
// the difference.
class Assign : public Stmt {
public:
    Assign(ExprPtr target, ExprPtr expr, int line)
        : Stmt(line), target_(std::move(target)), expr_(std::move(expr)) {}

    ExprPtr &target() { return target_; }
    ExprPtr &expr() { return expr_; }
    const Symbol *symbol() const { return symbol_; }
    void resolve(const Symbol *s) { symbol_ = s; }

    // True when the name did not exist and this assignment made it. An array
    // may be created this way only from a literal, which carries its own
    // shape; anything else has nothing to infer extents from.
    bool creates() const { return creates_; }
    void setCreates(bool value) { creates_ = value; }

    void accept(NodeVisitor &v) override { v.visit(*this); }

private:
    ExprPtr target_;
    ExprPtr expr_;
    const Symbol *symbol_ = nullptr;
    bool creates_ = false;
};

// 'x +: e' and 'x -: e'. Kept as its own statement rather than expanded into
// 'x : x + e' by the parser, because the target may be an element and the
// expansion would then need two copies of the index expression. The code
// generator walks the one target twice instead - once to read it and once to
// write it - which is also what the app's interpreter does.
class CompoundAssign : public Stmt {
public:
    CompoundAssign(ExprPtr target, bool add, ExprPtr expr, int line)
        : Stmt(line), target_(std::move(target)), expr_(std::move(expr)), add_(add) {}

    ExprPtr &target() { return target_; }
    ExprPtr &expr() { return expr_; }
    bool isAdd() const { return add_; }

    void accept(NodeVisitor &v) override { v.visit(*this); }

private:
    ExprPtr target_;
    ExprPtr expr_;
    bool add_;
};

// '?' prints its items and appends a newline; '??' prints them and does not.
// Each item is followed by a single space either way, so '?' always leaves a
// trailing space before its newline.
class Print : public Stmt {
public:
    Print(bool newline, int line) : Stmt(line), newline_(newline) {}

    void add(ExprPtr item) { items_.push_back(std::move(item)); }

    std::vector<ExprPtr> &items() { return items_; }
    const std::vector<ExprPtr> &items() const { return items_; }
    bool newline() const { return newline_; }
    void accept(NodeVisitor &v) override { v.visit(*this); }

private:
    std::vector<ExprPtr> items_;
    bool newline_;
};

// 'return', 'return e' or 'return (e, e, ...)'. A function that declares
// outputs must return them on every path; falling off the end is refused.
class Return : public Stmt {
public:
    explicit Return(int line) : Stmt(line) {}

    void add(ExprPtr expr) { exprs_.push_back(std::move(expr)); }
    std::vector<ExprPtr> &exprs() { return exprs_; }

    void accept(NodeVisitor &v) override { v.visit(*this); }

private:
    std::vector<ExprPtr> exprs_;
};

// '<a,b> : f(...)' - the only way to consume more than one returned value.
class MultiAssign : public Stmt {
public:
    MultiAssign(int line) : Stmt(line) {}

    void addTarget(std::string name) { names_.push_back(std::move(name)); }
    void setCall(ExprPtr call) { call_ = std::move(call); }

    const std::vector<std::string> &names() const { return names_; }
    std::vector<const Symbol *> &targets() { return targets_; }
    ExprPtr &call() { return call_; }

    void accept(NodeVisitor &v) override { v.visit(*this); }

private:
    std::vector<std::string> names_;
    std::vector<const Symbol *> targets_;
    ExprPtr call_;
};

// A call written where a statement belongs, its outputs discarded.
class CallStmt : public Stmt {
public:
    CallStmt(ExprPtr call, int line) : Stmt(line), call_(std::move(call)) {}

    ExprPtr &call() { return call_; }
    void accept(NodeVisitor &v) override { v.visit(*this); }

private:
    ExprPtr call_;
};

// 'if cond { } elseif cond { } else { }'. Any number of elseif branches, at
// most one else. The condition must be a scalar.
class If : public Stmt {
public:
    struct Branch {
        ExprPtr condition;
        Block body;
    };

    explicit If(int line) : Stmt(line) {}

    void addBranch(ExprPtr condition, Block body) {
        branches_.push_back(Branch());
        branches_.back().condition = std::move(condition);
        branches_.back().body = std::move(body);
    }
    void setElse(Block body) { elseBody_ = std::move(body); hasElse_ = true; }

    std::vector<Branch> &branches() { return branches_; }
    Block &elseBody() { return elseBody_; }
    bool hasElse() const { return hasElse_; }

    void accept(NodeVisitor &v) override { v.visit(*this); }

private:
    std::vector<Branch> branches_;
    Block elseBody_;
    bool hasElse_ = false;
};

class While : public Stmt {
public:
    While(ExprPtr condition, Block body, int line)
        : Stmt(line), condition_(std::move(condition)), body_(std::move(body)) {}

    ExprPtr &condition() { return condition_; }
    Block &body() { return body_; }
    void accept(NodeVisitor &v) override { v.visit(*this); }

private:
    ExprPtr condition_;
    Block body_;
};

// Both written forms arrive here. 'for i < n' is sugar and the parser expands
// it into 'for i : 0 to n - 1', which is why 'step' rides along unchanged and
// why nothing downstream knows the difference - including the fact that
// 'for i < 2.9' runs 0 to 1.9 and stops at 1.
//
// The counter belongs to the loop: it is created in a scope of its own and is
// gone afterwards, so it never collides with an outer name.
class For : public Stmt {
public:
    For(std::string variable, ExprPtr start, ExprPtr end, ExprPtr step, Block body, int line)
        : Stmt(line), variable_(std::move(variable)), start_(std::move(start)),
          end_(std::move(end)), step_(std::move(step)), body_(std::move(body)) {}

    const std::string &variable() const { return variable_; }
    ExprPtr &start() { return start_; }
    ExprPtr &end() { return end_; }
    ExprPtr &step() { return step_; }          // may be null, meaning 1
    Block &body() { return body_; }

    const Symbol *counter() const { return counter_; }
    void resolve(const Symbol *s) { counter_ = s; }
    // Four hidden slots, allocated together. A loop's bounds are evaluated
    // once and then consulted on every pass, so they have to live somewhere
    // for the length of the loop; and the pass number is kept rather than the
    // counter accumulated, because the interpreter computes start + n * step
    // and a compiler that accumulated would drift from it in the last digits.
    enum HiddenSlot { EndSlot, StepSlot, PassSlot, StartSlot, HiddenSlotCount };
    int hidden(HiddenSlot which) const { return hiddenBase_ + which; }
    void setHiddenBase(int base) { hiddenBase_ = base; }

    void accept(NodeVisitor &v) override { v.visit(*this); }

private:
    std::string variable_;
    ExprPtr start_;
    ExprPtr end_;
    ExprPtr step_;
    Block body_;
    const Symbol *counter_ = nullptr;
    int hiddenBase_ = 0;
};

// Both bind to the innermost enclosing loop and there are no labels, so
// neither can leave two loops at once. The parser refuses one outside a loop,
// which is why nothing downstream has to consider one arriving at function
// level.
class Break : public Stmt {
public:
    explicit Break(int line) : Stmt(line) {}
    void accept(NodeVisitor &v) override { v.visit(*this); }
};

// In a 'for', this still advances the counter: the step belongs to the loop,
// not to the body, so it skips the rest of the pass rather than the pass.
class Continue : public Stmt {
public:
    explicit Continue(int line) : Stmt(line) {}
    void accept(NodeVisitor &v) override { v.visit(*this); }
};

// ------------------------------------------------------------------- program

// What a function's frame has to hold. Filled in by the checker, which is
// already walking everything and is the only pass that knows both how many
// names a function declares and how deeply its expressions nest.
//
// One slot is eight bytes on every target, whatever it holds. A tagged value
// would need more and a packed one less, but a uniform slot is what lets the
// three emitters agree on an offset without a table. A name gets a slot of
// its own and never gives it back: a frame here is small, and reusing slots
// would buy nothing but a way to be wrong.
class Frame {
public:
    static const int slotBytes = 8;

    // Named places, and the hidden ones a loop or a call needs to hold
    // something for the length of a statement. The checker allocates these,
    // because it is the pass that knows the names.
    int addVariable() { return variables_++; }

    int variables() const { return variables_; }
    // Where the evaluation slots start. How many there are is the code
    // generator's answer and nobody else's - see Emitter::beginFunction.
    int evaluationBase() const { return variables_; }

private:
    int variables_ = 0;
};

// A parameter. An array is a reference always; a scalar is one only when it
// is written with '&'.
struct Param {
    std::string name;
    const Type *type = nullptr;
    bool byReference = false;
};

// 'fun <outputs> = name(inputs)'. The output list holds types, not names -
// in 2.x it held a variable's name and the function could fall off its end
// returning whatever that held, and both of those are gone.
struct Prototype {
    Prototype() = default;
    Prototype(std::string n, int l) : name(std::move(n)), line(l) {}

    std::string name;
    std::vector<const Type *> outputs;
    std::vector<Param> inputs;
    int line = 0;
    // Its place in the program, which is the index the runtime counts
    // recursion depth against.
    int id = 0;
    // Which source file it was written in. A function is wholly in one, which
    // is what lets the runtime be told once per call rather than once per
    // statement.
    int unit = 0;

    // Where a call puts its results. None: nothing. One: the accumulator.
    // More: the caller lends an address for each and the function returns
    // nothing, which is uniform and costs a store the single case avoids.
    bool returnsByPointer() const { return outputs.size() > 1; }
};

class Function {
public:
    Function(Prototype proto, Block body)
        : proto_(std::move(proto)), body_(std::move(body)) {}

    const Prototype &proto() const { return proto_; }
    Prototype &proto() { return proto_; }
    bool isCalled() const { return called_; }
    void markCalled() { called_ = true; }

    // A definition the collection pass refused - a built-in's name, 'prec',
    // or a second function of the same name. It is not a function of the
    // program, so nothing further is said about it.
    bool isRejected() const { return rejected_; }
    void reject() { rejected_ = true; }
    Block &body() { return body_; }
    const Block &body() const { return body_; }
    Frame &frame() { return frame_; }
    const Frame &frame() const { return frame_; }

    // The function owns every name declared inside it, however deeply. A
    // symbol outlives the scope that introduced it because the frame slot
    // does: lifetimes here are the whole call.
    Symbol *declare(const std::string &name, const Type *type) {
        symbols_.push_back(std::unique_ptr<Symbol>(
            new Symbol(name, type, frame_.addVariable())));
        return symbols_.back().get();
    }

    // A place in the frame with no name: a loop's pass counter. It is a
    // variable slot rather than an evaluation one because it has to survive
    // the whole loop, which every expression inside it may use.
    int addHiddenSlot() { return frame_.addVariable(); }

    // Where the addresses of a multi-output function's results are kept. The
    // caller lends one per output; they arrive as arguments after the
    // declared ones and are spilled like any other.
    int outPointerBase() const { return outPointerBase_; }
    void setOutPointerBase(int base) { outPointerBase_ = base; }

    // Where a single return value waits between the 'return' that computed
    // it and the epilogue. It cannot stay in the accumulator: the epilogue
    // has a call of its own to make - the recursion counter comes down there
    // - and a call is exactly what does not preserve an accumulator.
    int resultSlot() const { return resultSlot_; }
    void setResultSlot(int slot) { resultSlot_ = slot; }

private:
    Prototype proto_;
    Block body_;
    Frame frame_;
    std::vector<std::unique_ptr<Symbol>> symbols_;
    bool called_ = false;
    bool rejected_ = false;
    int outPointerBase_ = 0;
    int resultSlot_ = -1;
};

// Execution begins at main(), which takes no inputs. Definitions may be
// written in any order.
class Program {
public:
    // Top-level order is kept, because it is meaning: a global is visible
    // below the line that declares it and nowhere above it, and a function
    // written above a global cannot see it. Functions themselves are not
    // ordered this way - they are collected before any body is checked - but
    // the checker still walks the file in order, because the interpreter
    // creates the globals in file order too and a checker that disagreed
    // would pass programs the run then failed.
    void add(std::unique_ptr<Function> f) {
        order_.push_back(Entry{true, functions_.size()});
        functions_.push_back(std::move(f));
    }

    void addGlobal(StmtPtr declaration) {
        order_.push_back(Entry{false, globals_.size()});
        globals_.push_back(std::move(declaration));
    }

    struct Entry { bool isFunction; size_t index; };

    std::vector<std::unique_ptr<Function>> &functions() { return functions_; }
    const std::vector<std::unique_ptr<Function>> &functions() const { return functions_; }
    std::vector<StmtPtr> &globals() { return globals_; }
    const std::vector<Entry> &order() const { return order_; }

    const Function *find(const std::string &name) const;
    Function *find(const std::string &name);

    // The block of eight-byte places every global lives in.
    int addGlobalSlot() { return globalSlots_++; }
    int globalSlots() const { return globalSlots_; }

    // The frame the initializers run in: they are ordinary statements and
    // need somewhere to compute in.
    Function &initializer() { return initializer_; }

private:
    std::vector<std::unique_ptr<Function>> functions_;
    std::vector<StmtPtr> globals_;
    std::vector<Entry> order_;
    int globalSlots_ = 0;
    Function initializer_{Prototype(), Block()};
};

}  // namespace shalimar
