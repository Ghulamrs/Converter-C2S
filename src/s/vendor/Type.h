// The four types, and nothing else.
//
//   int     32-bit signed whole number
//   real    64-bit floating point
//   char    one byte, 0..255
//   T[]     array of T, any rank
//
// char[] is text. There are no booleans, no closures and no array of strings -
// char is one-dimensional by rule, so 'char g[2][4]' is refused.
//
// Types are interned and compared by pointer. There are three scalars and one
// array constructor, so the whole universe of them a program can name is a
// handful of objects; giving them identity is cheaper than comparing trees and
// makes 'a reference must match exactly' a pointer comparison.
#pragma once

#include <string>

namespace shalimar {

class Type {
public:
    enum class Kind { Int, Real, Char, Array };

    static const Type *intType();
    static const Type *realType();
    static const Type *charType();
    static const Type *arrayOf(const Type *element);

    Kind kind() const { return kind_; }
    bool isArray() const { return kind_ == Kind::Array; }
    bool isScalar() const { return kind_ != Kind::Array; }

    // Array only; null for a scalar.
    const Type *element() const { return element_; }

    int rank() const { return isArray() ? 1 + element_->rank() : 0; }

    // The scalar at the bottom of however many array layers.
    const Type *scalar() const { return isArray() ? element_->scalar() : this; }

    // char[] is text and text is a line, so a char array above rank 1 has no
    // meaning to give. Every other rank is well formed.
    bool isWellFormed() const {
        return !(scalar()->kind() == Kind::Char && rank() > 1);
    }

    // 'real[]', 'int', 'char[][]' - what a diagnostic quotes back.
    std::string spelling() const;

private:
    Type(Kind kind, const Type *element) : kind_(kind), element_(element) {}

    Kind kind_;
    const Type *element_;
};

}  // namespace shalimar
