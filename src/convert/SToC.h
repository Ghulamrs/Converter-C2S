#ifndef C2S_CONVERT_STOC_H
#define C2S_CONVERT_STOC_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "../c/CAst.h"
#include "../s/vendor/Ast.h"

namespace c2s {

class Source;
class Diagnostics;

// Shalimar to C89: a visitor over the checked Shalimar tree that builds a
// C tree, printed afterwards by CPrinter.
//
// This is one half of a symmetric pair - CToS is the other - and the two
// follow the same design on purpose: each is a NodeVisitor over its source
// language's AST, each builds the target language's AST rather than text,
// each hands the finished tree to that language's printer, and each marks a
// construct it cannot carry with the same #BEYOND SHALIMAR comment block,
// quoting the original source, rather than refusing the file.
//
// The tree converted here must have been through Compiler-S's Checker:
// every expression carries its type, conversions are explicit Convert
// nodes, and symbols are resolved. That is what lets this stay a walk
// rather than a second type checker.
//
// What the generated C is, and is not:
//
//   - Arrays are flattened: a Shalimar array travels as a pointer plus one
//     int per dimension ('double *R, int R_d0, int R_d1'), locals with
//     constant extents become one C array of the product, and indexing is
//     linearised. '.row', '.col' and '.dim(n)' read the dimension values.
//   - A function with several outputs returns void and fills out-pointers;
//     '<a,b> : f(x)' becomes 'f(x, &a, &b)'.
//   - Printing reproduces shc's runtime byte for byte - the space after
//     every item, the 7-place scalars and 6-place grids, the right-aligned
//     columns - through small static helpers emitted into the file.
//   - Shalimar's checked int arithmetic is NOT reproduced: C's int
//     arithmetic is used as C defines it, so a program that would abort in
//     shc with 'int overflow' wraps silently in the C translation. That is
//     the one semantic deviation, and it is deliberate.
class SToC : public shalimar::NodeVisitor {
public:
    SToC(const Source &source, Diagnostics &diagnostics);

    // The C tree, always - gaps become CBeyond markers, counted in
    // beyondCount(). Callers decide what a gap count means for exit status.
    std::unique_ptr<CProgram> convert(shalimar::Program &program);

    int beyondCount() const { return beyondCount_; }

    // The emitted runtime: the static helper functions the converted
    // program needs, in dependency order, ready to paste above it. Empty
    // when nothing used them.
    std::string preamble() const;

    // The #include lines the output needs.
    std::vector<std::string> includes() const;

    // NodeVisitor - expressions set expr_, statements append to block_.
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
    // What this converter knows about one Shalimar name.
    struct Info {
        std::string cName;
        int rank = 0;                        // 0 for a scalar
        std::vector<long long> extents;      // constant extents, locals only
        bool isParamArray = false;           // dims travel as <name>_dN
        bool isRefScalar = false;            // '&x: int' - a C pointer
    };

    // ---- the shared converter vocabulary (same names in CToS) ----
    void convertFunction(shalimar::Function &fn);
    void statement(shalimar::Stmt &node);
    CExprPtr expression(shalimar::Expr &node);
    void block(shalimar::Block &body, CCompound &into);
    void markBeyond(int line, const std::string &reason);

    // ---- helpers of this direction ----
    const Info *infoFor(const shalimar::Symbol *symbol) const;
    const Info *lookupVar(const shalimar::Var &var);
    static std::string cEscape(const std::string &text);
    std::string freshName(const std::string &base);
    static std::string sanitise(const std::string &name);
    bool foldInt(shalimar::Expr &node, long long *out) const;
    CExprPtr linearIndex(shalimar::Expr &chain, int *outRankLeft,
                         const Info **outInfo);
    CExprPtr dimValue(const Info &info, int axis);
    CTypePtr scalarC(const shalimar::Type *type) const;
    CExprPtr callHelper(const std::string &name, std::vector<CExprPtr> args);
    void need(const std::string &helper);
    void printItem(shalimar::Expr &item);
    CStmtPtr declStmtFor(const std::string &name, CTypePtr type, CExprPtr init);

    const Source &source_;
    Diagnostics &diagnostics_;

    std::unique_ptr<CProgram> program_;
    CCompound *block_ = nullptr;             // where statements land
    CExprPtr expr_;                          // the last expression built
    shalimar::Function *currentFn_ = nullptr;
    shalimar::Program *sProgram_ = nullptr;

    std::map<const shalimar::Symbol *, Info> symbols_;
    std::map<std::string, Info> paramInfos_;   // this function's parameters, by name
    std::map<std::string, bool> helpers_;    // which preamble pieces are used
    int beyondCount_ = 0;
    int tempCount_ = 0;
    bool usesPrint_ = false;
    bool usesMath_ = false;
    bool usesStdlib_ = false;
    bool currentIsMain_ = false;
};

}  // namespace c2s

#endif
