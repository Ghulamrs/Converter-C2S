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
