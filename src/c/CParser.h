#ifndef C2S_C_CPARSER_H
#define C2S_C_CPARSER_H

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "CAst.h"
#include "CToken.h"

namespace c2s {

class Source;
class Diagnostics;

// Tokens to a source-faithful C89 tree.
//
// Recursive descent over the same grammar Compiler-C's parser reads, with
// its two deliberate strictnesses kept - no K&R definitions, no trigraphs -
// and one large difference of purpose: this parser preserves, where cc1
// lowers. Nothing is folded, nothing decays, and every declaration survives.
//
// Because no preprocessor ran, nothing from a header exists. That is fine
// for the grammar with one exception C is famous for: a typedef name is only
// recognisable as a type by knowing it was one. Typedefs from the file
// itself are tracked; a typedef that lived in a header is unknown, and a
// declaration written with it will not parse - which surfaces as an honest
// syntax error naming the identifier, and is the intended fate of a program
// leaning on header types it never showed the converter.
//
// A syntax error stops the parse - unlike a conversion error, of which every
// one is collected. The distinction is deliberate: a file that does not
// parse is not a C89 program, cc1 will say so in its own words, and a parse
// forced past its first error reports phantoms after it.
class CParser {
public:
    CParser(const Source &source, std::vector<CToken> tokens,
            Diagnostics &diagnostics);

    // Null when the input did not parse; the reason is in the diagnostics.
    std::unique_ptr<CProgram> parse();

private:
    // ------------------------------------------------------------ plumbing
    const CToken &current() const;
    const CToken &peek(std::size_t ahead) const;
    void advance();
    bool at(const char *text) const;
    bool atKeyword(const char *text) const;
    bool accept(const char *text);
    bool expect(const char *text, const char *where);
    void fail(const std::string &message);
    bool failed() const { return failed_; }

    bool atTypeStart() const;
    bool isTypedefName(const std::string &name) const;

    // -------------------------------------------------------- declarations
    struct Specifiers {
        CDeclaration::Storage storage = CDeclaration::Storage::None;
        CTypePtr type;
    };

    bool declarationSpecifiers(Specifiers *out, bool *sawAny);
    CTypePtr typeSpecifier(bool *sawType, bool *isConst, bool *isVolatile,
                           bool *isUnsigned, bool *isSignedWord,
                           bool *isShort, int *longCount);
    CTypePtr structOrUnion();
    CTypePtr enumSpecifier();

    // Wraps 'base' with the pointer/array/function shape of one declarator
    // and yields the name. An abstract declarator has no name.
    bool declarator(CTypePtr base, std::string *name, CTypePtr *out,
                    bool abstractAllowed);
    bool directDeclarator(CTypePtr base, std::string *name, CTypePtr *out,
                          bool abstractAllowed);
    bool parameterList(CType *fn);
    CTypePtr typeName();               // for casts and sizeof

    std::unique_ptr<CDeclaration> declaration(Specifiers specifiers,
                                              bool *wasFunctionDef,
                                              std::unique_ptr<CFunctionDef> *fnOut);
    bool initializer(CInit *out);

    // ---------------------------------------------------------- statements
    CStmtPtr statement();
    CStmtPtr compound();
    CStmtPtr blockItem();

    // --------------------------------------------------------- expressions
    CExprPtr expression();             // includes the comma operator
    CExprPtr assignment();
    CExprPtr conditional();
    CExprPtr binary(int minPrecedence);
    CExprPtr castExpression();
    CExprPtr unary();
    CExprPtr postfix();
    CExprPtr primary();

    const Source &source_;
    std::vector<CToken> tokens_;
    Diagnostics &diagnostics_;
    std::size_t index_ = 0;
    bool failed_ = false;

    // Typedef names seen so far, per scope; the outermost set is file scope.
    std::vector<std::set<std::string>> typedefScopes_;
};

}  // namespace c2s

#endif
