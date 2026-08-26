#include "Builtin.h"

#include <cmath>
#include <cstring>

namespace shalimar {
namespace {

const Builtin table[] = {
    {"abs",   1, Builtin::Shape::IntOrReal, "shm_fn_abs_real", "shm_fn_abs_int"},
    {"sqrt",  1, Builtin::Shape::Real,      "shm_fn_sqrt",     nullptr},
    {"log",   1, Builtin::Shape::Real,      "shm_fn_log",      nullptr},
    {"exp",   1, Builtin::Shape::Real,      "shm_fn_exp",      nullptr},
    {"hypot", 2, Builtin::Shape::Real,      "shm_fn_hypot",    nullptr},
    {"sin",   1, Builtin::Shape::Real,      "shm_fn_sin",      nullptr},
    {"cos",   1, Builtin::Shape::Real,      "shm_fn_cos",      nullptr},
    {"tan",   1, Builtin::Shape::Real,      "shm_fn_tan",      nullptr},
    {"asin",  1, Builtin::Shape::Real,      "shm_fn_asin",     nullptr},
    {"acos",  1, Builtin::Shape::Real,      "shm_fn_acos",     nullptr},
    {"atan",  1, Builtin::Shape::Real,      "shm_fn_atan",     nullptr},
    {"atan2", 2, Builtin::Shape::Real,      "shm_fn_atan2",    nullptr},
    {"pow",   2, Builtin::Shape::Real,      "shm_fn_pow",      nullptr},
    {"round", 1, Builtin::Shape::Real,      "shm_fn_round",    nullptr},
    {"ceil",  1, Builtin::Shape::Real,      "shm_fn_ceil",     nullptr},
    {"floor", 1, Builtin::Shape::Real,      "shm_fn_floor",    nullptr},

    {"trunc", 1, Builtin::Shape::Real,      "shm_fn_trunc",    nullptr},
    {"max",   2, Builtin::Shape::IntOrReal, "shm_fn_max_real", "shm_fn_max_int"},
    {"min",   2, Builtin::Shape::IntOrReal, "shm_fn_min_real", "shm_fn_min_int"},

    {"len",   1, Builtin::Shape::Length,    nullptr,           nullptr}
};

const int count = static_cast<int>(sizeof table / sizeof table[0]);

}

// Names a person will reasonably try, and the reason each one cannot be
// borrowed. Not a blocklist: every entry here is refused because its C
// signature needs a type Shalimar does not have, and saying which type is the
// difference between an answer and a refusal. docs/FOREIGN.md explains why
// the boundary falls exactly here.
//
// The list is short on purpose. It exists to turn the most likely mistakes
// into instructions; anything not on it still gets a plain "not a library
// function this compiler knows", which is true and not misleading.
namespace {
struct Unborrowable { const char *name; const char *why; };
const Unborrowable kUnborrowable[] = {
    {"memset",  "takes a pointer, which Shalimar has no type for"},
    {"memcpy",  "takes a pointer, which Shalimar has no type for"},
    {"strlen",  "takes a pointer, which Shalimar has no type for"},
    {"strcpy",  "takes a pointer, which Shalimar has no type for"},
    {"strcmp",  "takes a pointer, which Shalimar has no type for"},
    {"malloc",  "returns a pointer, which Shalimar has no type for"},
    {"free",    "takes a pointer, which Shalimar has no type for"},
    {"fopen",   "returns a pointer, which Shalimar has no type for"},
    {"qsort",   "takes a function pointer, which Shalimar has no type for"},
    {"printf",  "takes a variable number of arguments, which Shalimar has no form for"},
    {"scanf",   "takes a variable number of arguments, which Shalimar has no form for"},
    {"modf",    "writes through a pointer; a two-output 'fun' is the Shalimar shape for it"},
    {"frexp",   "writes through a pointer; a two-output 'fun' is the Shalimar shape for it"},
};
}

const char *whyNotBorrowable(const std::string &name) {
    for (std::size_t i = 0; i < sizeof kUnborrowable / sizeof kUnborrowable[0]; ++i)
        if (name == kUnborrowable[i].name) return kUnborrowable[i].why;
    return nullptr;
}

int findBuiltin(const std::string &name) {
    for (int i = 0; i < count; ++i) {
        if (name == table[i].name) return i;
    }
    return -1;
}

const Builtin &builtin(int index) { return table[index]; }
int builtinCount() { return count; }

bool isConstant(const std::string &name) { return name == "pi" || name == "e"; }

double constantValue(const std::string &name) {
    return name == "pi" ? 3.141592653589793 : 2.718281828459045;
}

}
