// The R face of the converter - and nothing more.
//
// One .Call entry, no dependencies beyond R's own C API. Everything of
// substance lives in Converter (core/Converter.h), which does no I/O; this
// file moves strings across the R boundary and shapes the Result as an R
// list. The command-line tool is the other caller of the same class.
//
// R and the core are both built by R CMD INSTALL through src/Makevars, with
// the core sources copied under core/ by the repository's 'make rpkg'.

#include <string>
#include <vector>

#include <R.h>
#include <Rinternals.h>

#include "core/Converter.h"

namespace {

SEXP characterVector(const std::vector<std::string> &items) {
    SEXP out = PROTECT(Rf_allocVector(STRSXP, static_cast<R_xlen_t>(items.size())));
    for (std::size_t i = 0; i < items.size(); ++i) {
        SET_STRING_ELT(out, static_cast<R_xlen_t>(i),
                       Rf_mkCharLen(items[i].c_str(),
                                    static_cast<int>(items[i].size())));
    }
    UNPROTECT(1);
    return out;
}

std::string asString(SEXP value, const char *what) {
    if (TYPEOF(value) != STRSXP || Rf_xlength(value) < 1) {
        Rf_error("%s must be a character string", what);
    }
    const char *text = CHAR(STRING_ELT(value, 0));
    return std::string(text);
}

}  // namespace

extern "C" {

// .Call("c2s_convert_call", text, name, direction)
//
// direction: "auto", "c-to-shalimar", "shalimar-to-c", "canon".
// Returns list(ok, output, beyond, questions, messages, summary).
SEXP c2s_convert_call(SEXP textSexp, SEXP nameSexp, SEXP directionSexp) {
    const std::string text = asString(textSexp, "text");
    const std::string name = asString(nameSexp, "name");
    const std::string direction = asString(directionSexp, "direction");

    c2s::Converter::Result result;
    if (direction == "auto") {
        result = c2s::Converter::convertFile(text, name);
    } else if (direction == "c-to-shalimar") {
        result = c2s::Converter::convert(text, name, c2s::Direction::CToShalimar);
    } else if (direction == "shalimar-to-c") {
        result = c2s::Converter::convert(text, name, c2s::Direction::ShalimarToC);
    } else if (direction == "canon-c") {
        result = c2s::Converter::canonicalise(text, name,
                                              c2s::Direction::CToShalimar);
    } else if (direction == "canon-shalimar") {
        result = c2s::Converter::canonicalise(text, name,
                                              c2s::Direction::ShalimarToC);
    } else {
        Rf_error("unknown direction '%s'", direction.c_str());
    }

    std::vector<std::string> messages;
    for (std::size_t i = 0; i < result.diagnostics.size(); ++i) {
        messages.push_back(result.diagnostics[i].formatted());
    }

    const char *names[] = {"ok", "output", "beyond", "questions",
                           "messages", "summary", ""};
    SEXP out = PROTECT(Rf_mkNamed(VECSXP, names));
    SET_VECTOR_ELT(out, 0, Rf_ScalarLogical(result.ok ? TRUE : FALSE));
    SET_VECTOR_ELT(out, 1, Rf_ScalarString(Rf_mkCharLen(
        result.output.c_str(), static_cast<int>(result.output.size()))));
    SET_VECTOR_ELT(out, 2, Rf_ScalarInteger(result.beyondCount));
    SET_VECTOR_ELT(out, 3, characterVector(result.questions));
    SET_VECTOR_ELT(out, 4, characterVector(messages));
    SET_VECTOR_ELT(out, 5, Rf_ScalarString(Rf_mkCharLen(
        result.summary.c_str(), static_cast<int>(result.summary.size()))));
    UNPROTECT(1);
    return out;
}

}  // extern "C"
