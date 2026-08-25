#ifndef C2S_CONVERT_CTOS_H
#define C2S_CONVERT_CTOS_H

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "../Options.h"
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
//   - Counting 'for' loops become 'for i : a to b step k', but only where
//     nothing reads the counter after the loop - Shalimar's for binds its
//     own, and C's leaves the variable holding what ended the loop. That
//     case, and any other shape, lower to a while with the step at the
//     body's end, which assigns the variable C's way.
//   - Block-scoped declarations hoist to the top of the function, renamed
//     where scopes collided; C block structure flattens away.
//   - 'x ? a : b' becomes an if / else in the two places there is somewhere
//     to put the branch: as the whole right side of an assignment, writing
//     the target in each arm, and as the whole of a return, returning in
//     each arm. A chain of them flattens into 'elseif'. Anywhere else - in
//     a call argument, inside a larger expression - there is no statement
//     to expand into and it is a marker.
//   - '++', '--' and the compound assignments become '+:', '-:' or
//     spelled-out assignments; '!x' becomes 'x = 0'.
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
    CToS(const Source &source, Diagnostics &diagnostics,
         const Permissions &permissions = Permissions());

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

    // --allow-short-circuit. Shalimar's '&' and '|' ask both sides before
    // either is answered, so an impure right side - an index, a call, a
    // division - cannot simply become one of them. The rewrite is a
    // temporary and a nest of ifs, which are statements, so it only works
    // where there is somewhere to put statements. 'canLift_' is how a
    // statement visitor says "here is such a place" to the expression it is
    // about to convert; 'liftable_' is that answer as the visitor reads it,
    // latched by expression() before any nested call can overwrite it.
    // Anything the rewrite emits goes in 'lifted_', to be flushed ahead of
    // the statement being built.
    void flushLifted();
    std::string mintLiftTemp(const shalimar::Type *type);
    bool lowerShortCircuit(CBinary &node);
    bool canLift_ = false;
    bool liftable_ = false;
    std::vector<shalimar::StmtPtr> lifted_;
    // This function's lifted temporaries, with the type each was minted
    // at, declared at the top of the function once its walk is done.
    std::vector<std::pair<std::string, const shalimar::Type *> > liftTemps_;
    bool isCharContext(CExpr &other) const;

    // Does this expression stand for a whole array rather than one of its
    // elements? A bare array name, a string literal, or a partial index of
    // something with more dimensions than were given.
    bool isArrayValued(CExpr &node) const;
    bool isCharValued(CExpr &node) const;
    shalimar::ExprPtr charWrap(shalimar::ExprPtr value);
    shalimar::ExprPtr intWrap(shalimar::ExprPtr value);
    void lowerPrintf(CCall &call);

    // The Shalimar function that writes one printf's format, made on first
    // use and shared by every later call with the same format and the same
    // parameter types. Returns its name.
    std::string printFunction(const std::string &format,
                              const std::vector<shalimar::Param> &params,
                              shalimar::Block body, int line);
    // Returns false when the general while lowering must run instead. The
    // out parameter separates the two reasons for that: it is set to the
    // counter's name when the shape did count and only the counter being
    // read afterwards ruled the form out, and left empty otherwise. The
    // caller's diagnostic is a different sentence in each case.
    bool lowerCountingFor(CFor &node, std::string *escapedCounter);
    bool counterEscapes(CFor &node, const std::string &name) const;

    // Is this name bound at file scope rather than inside the function
    // being converted? scopes_[0] is the file's, so a name that resolves
    // there and nowhere nearer belongs to the whole program.
    bool isFileScope(const std::string &name) const;
    // One 'case' or 'default' of a switch, with the statements that follow
    // it up to the next label. Named here rather than inside lowerSwitch
    // because the falling lowering is a second function over the same list.
    // The names one switch needs at the top of the function. 'selector'
    // always - the value being tested, saved once so the chain can test it
    // repeatedly. 'entry' and 'done' only under --allow-fall-through, which
    // trades the if/elseif chain for a pair of flags that can express a case
    // running on into the next; they are minted here regardless of whether
    // this particular switch turns out to need them, because the hoist walk
    // reaches the top of the function and the statement walk does not.
    struct SwitchTemps {
        std::string selector;
        std::string entry;
        std::string done;
    };

    struct SwitchArm {
        std::vector<shalimar::ExprPtr> values;   // empty for default
        bool isDefault = false;
        std::vector<CStmt *> body;
        std::size_t offset = 0;
    };
    void lowerSwitch(CSwitch &node);

    // The --allow-fall-through lowering, used only when some arm really does
    // run on into the next. It gives up the if/elseif chain - which cannot
    // say 'and then the one after' - for an entry index and a done flag.
    void lowerFallingSwitchArms(CSwitch &node, const SwitchTemps &names,
                                std::vector<SwitchArm> &arms,
                                const std::vector<bool> &terminates,
                                bool wrapped);

    // A switch that some arm leaves from the middle is wrapped in a loop
    // that always ends its first turn, so that C's 'break' has a Shalimar
    // loop of its own to leave - see lowerSwitch.
    void closeSwitchWrapper(CSwitch &node, bool needsWrapper,
                            shalimar::Block *outerBlock,
                            shalimar::Block &wrapped);
    bool lowerTernaryReturn(CTernary &top, std::size_t offset,
                            shalimar::Block *into);
    bool returnArm(CExpr &value, std::size_t offset, shalimar::Block *into);
    void hoistDeclarations(CStmt &node, shalimar::Block *top);
    void convertTopDeclaration(CDeclaration &decl);
    std::size_t declOffset(CDeclaration &decl) const;
    int lineOf(std::size_t offset) const;
    std::vector<std::string> sourceLinesAt(std::size_t offset) const;

    // No Diagnostics is held. A construct with no expression in the target
    // language is refused by markBeyond - a marker in the tree and a count -
    // rather than by a message on the side, so there is nothing here to
    // report through. The constructor still takes one for the day that
    // changes, and for the symmetry of the two directions.
    const Source &source_;

    // The rewrites the command line allowed. Each one of these compiles
    // without meaning quite what the C did, which is why none is the
    // default and why each is asked for by name - see Options.h.
    Permissions permissions_;

    std::unique_ptr<shalimar::Program> program_;
    shalimar::Block *block_ = nullptr;
    shalimar::ExprPtr expr_;
    bool currentIsMain_ = false;

    // Whether the function being converted answers with a char. A return's
    // value has to be wrapped to match, and only this says so - the C tree
    // carries no types, so the return statement cannot work it out alone.
    bool currentReturnsChar_ = false;
    CFunctionDef *currentFn_ = nullptr;   // for the whole-function scans
    int loopDepth_ = 0;

    // Name scopes: each maps a C name to its Info; lookups walk outward.
    std::vector<std::map<std::string, Info>> scopes_;

    // What the hoist walk decided about each local, keyed by the source
    // offset of its declarator, and the selector name it minted for each
    // switch, keyed by the switch's offset.
    //
    // The hoist has to run first - Shalimar wants every Declare at the top
    // of the function, so the names must exist before a statement can use
    // one. But it walks the function flat, with no scope stack, so it is in
    // no position to say which scope a name belongs to. Registering there
    // put every local in one map, where an inner shadow overwrote the outer
    // entry and both names then resolved to whichever came last. The names
    // were distinct in the output and the bindings were not, which is the
    // shape of bug that compiles and runs and prints the wrong number.
    //
    // So the hoist parks its findings here, and the statement walk registers
    // each one into the scope it is actually in, at the point the
    // declaration stands - which is where C says the name becomes visible,
    // and where block() will pop it again.
    std::map<std::size_t, Info> hoisted_;

    std::map<std::size_t, SwitchTemps> switchTemps_;
    std::set<std::string> usedNames_;      // every Shalimar name handed out
    std::set<std::string> knownFunctions_; // defined in this file

    // Format text and parameter types to the printing function built for
    // them, so a printf inside a loop makes one function and not one per
    // turn of the source it was written in.
    std::map<std::string, std::string> printFunctions_;
    int printCount_ = 0;
    int beyondCount_ = 0;
    int tempCount_ = 0;
};

}  // namespace c2s

#endif
