#ifndef C2S_CONVERTER_H
#define C2S_CONVERTER_H

#include <string>
#include <vector>

#include "Diagnostics.h"
#include "Options.h"

namespace c2s {

// The converter as a library - the product's real surface.
//
// Everything below is a pure function of its inputs: no file is opened
// beyond what the caller hands over as text, nothing is printed, nothing
// exits, and every message comes back as data. The command-line tool is one
// thin caller of this class; an RStudio addin is another; both see exactly
// the same behaviour because there is nothing else the behaviour could live
// in.
//
// The preprocessor decision flow is an API round trip. When a C source
// carries #define, #if and their kin, convert() does not guess: it returns
// ok == false with the questions() list filled - one entry per directive,
// as written - and no output. The caller shows the list, the user resolves
// each one in the source, and the next convert() call proceeds. That is the
// no-console shape of "list them, then ask which way we go".
class Converter {
public:
    struct Result {
        // The conversion ran and produced usable output. Markers do not
        // clear this flag - see beyondCount - but syntax errors and pending
        // preprocessor questions do.
        bool ok = false;

        // The converted program. Empty when ok is false.
        std::string output;

        // How many #BEYOND SHALIMAR comment breaks interrupt the output:
        // constructs of the source language with no expression in the
        // target, each quoting the original source where it stands.
        int beyondCount = 0;

        // Every message the run produced, in the order found.
        std::vector<Diagnostic> diagnostics;

        // The preprocessor constructs awaiting a decision, as written, in
        // file order. Non-empty means the conversion did not start.
        std::vector<std::string> questions;

        // One line: "2 syntax errors", "no diagnostics", ...
        std::string summary;
    };

    // Converts source text. 'name' labels diagnostics ("prime.c"); it is
    // not opened. The direction says which language the text is in.
    static Result convert(const std::string &sourceText, const std::string &name,
                          Direction direction,
                          const Permissions &permissions = Permissions(),
                          bool emitIncludes = true);

    // Convenience: infer the direction from the name's extension. Returns
    // ok == false with a diagnostic when the extension says nothing.
    static Result convertFile(const std::string &sourceText, const std::string &name,
                              const Permissions &permissions = Permissions());

    // Parse with the input language's own front end and print it back in
    // canonical form, converting nothing. The identity that proves a front
    // end and its printer against the real compiler; the differential test
    // suite is built on it.
    static Result canonicalise(const std::string &sourceText, const std::string &name,
                               Direction direction);
};

}  // namespace c2s

#endif
