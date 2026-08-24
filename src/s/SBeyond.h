#ifndef C2S_S_SBEYOND_H
#define C2S_S_SBEYOND_H

#include <string>
#include <vector>

#include "vendor/Ast.h"

namespace c2s {

// A C89 construct with no Shalimar expression: the break in the output the
// user asked for, tagged #BEYOND SHALIMAR and quoting the original C, so
// the rest of the program still comes through and the gaps are visible
// exactly where they belong.
//
// Compiler-S's NodeVisitor is a closed list this converter must not edit,
// so this node dispatches to nothing - accept() is empty - and SPrinter
// recognises it by type before dispatching. That is the one asymmetry with
// the C side, where CBeyond is an ordinary visited node; the printed shape
// and the meaning are identical.
class SBeyondStmt : public shalimar::Stmt {
public:
    SBeyondStmt(std::string reason, std::vector<std::string> sourceLines, int line)
        : shalimar::Stmt(line), reason_(std::move(reason)),
          lines_(std::move(sourceLines)) {}

    const std::string &reason() const { return reason_; }
    const std::vector<std::string> &lines() const { return lines_; }

    void accept(shalimar::NodeVisitor &) override {}

private:
    std::string reason_;
    std::vector<std::string> lines_;
};

}  // namespace c2s

#endif
