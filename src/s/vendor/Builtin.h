// The built-in functions and the two constants.
//
// A table rather than a class each: they differ only in how many arguments
// they take and whether an int argument keeps the answer an int, and a table
// of that is easier to read against the reference than a dozen small classes
// would be.
//
// Argument counts are enforced exactly, as for a user function, and a user
// function may not take one of these names.
#pragma once

#include "Type.h"

#include <string>

namespace shalimar {

struct Builtin {
    // How the answer's type follows from the arguments'.
    enum class Shape {
        Real,        // reals in, real out, whatever was written
        IntOrReal,   // int in, int out; otherwise real
        Length       // an array in, an int out
    };

    const char *name;
    int arity;
    Shape shape;
    const char *realSymbol;
    const char *intSymbol;      // Shape::IntOrReal only
};

// The index of the built-in with this name, or -1.
int findBuiltin(const std::string &name);
const Builtin &builtin(int index);
int builtinCount();

// 'pi' and 'e' are read-only and reserved: neither can be declared, assigned,
// taken as a parameter name, or used as a loop counter. One name, one
// meaning - in 2.x a variable of the same name shadowed them, which cannot
// survive a checker that defines a variable on first assignment.
bool isConstant(const std::string &name);
double constantValue(const std::string &name);

}  // namespace shalimar
