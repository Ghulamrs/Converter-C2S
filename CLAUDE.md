# CLAUDE.md

Guidance for Claude working in this repository.

## What this is

A two-way source converter between C89 and Shalimar. `../Compiler-C` defines
what C89 means here and `../Compiler-S` defines what Shalimar means here.
Where this converter and one of those compilers disagree, **the compiler
wins** — it is the thing that will actually be handed the output.

`../Shalimar/SHALIMAR_LANGUAGE.md` is the language's specification and
`../Compiler-S/docs/CONFORMANCE.md` records where `shc` and the app's
interpreter differ. When emitting Shalimar, target **`shc`**: the output has to
compile, and `shc` is what compiles it.

## The language this is written in

`src/` is **ISO C++14**, at `-std=c++14 -Wall -Wextra -Werror -pedantic`, for
the same reason both compilers are: three toolchains have to accept it.

Two traps inherited from those repositories, both real:

- **A Mac cannot enforce C++14.** Apple's libc++ hands you C++17 names under
  `-std=c++14`. Only a real g++ says whether the sources are C++14.
- **Trigraphs are still live in C++14**, and `??` is a Shalimar token this
  converter emits. Write it `"?\?"` in every C++ string literal, or
  `-pedantic` turns `"??"` into something else.

## The rule about the neighbours

**Neither `../Compiler-C` nor `../Compiler-S` is modified, and neither is
linked.** Their sources are not compiled into this tree. Both grammars are
transcribed here instead, and the suite keeps the transcription honest by
running the real compilers over every input and every output.

That decision has a cost worth naming: two C89 grammars now exist in this
workspace and grammars drift. The mitigation is the suite, not care. If a case
is added that this converter accepts and `cc1` refuses — or the reverse — that
is a bug here, not a curiosity.

Two consequences of not linking, both deliberate:

- `Compiler-C`'s `Source::fail` reaches `std::exit`, and it has no warnings
  and no diagnostic collection. Nothing of that shape is inherited: every
  message here is collected, the whole file is walked, and the run reports all
  of them at once.
- `Compiler-C`'s AST is a code-generation tree — declarations are erased,
  `a[i]` is already `*(a + i*8)`, `enum` and `sizeof` are folded away. This
  converter's `CAst` is **source-faithful** on purpose. Do not lower anything
  in the parser: what the parser builds is what the author wrote, so a
  diagnostic can quote it and `--canon` can print it back. Lowering belongs
  to `convert/`, and nowhere earlier.

## Architecture

```
a.c   -> c/CPreScan -> c/CLexer -> c/CParser -> CAst
                                                 |
                                    convert/CToS -> SAst -> s/SPrinter -> a.shm

b.shm -> s/SFrontEnd -> SAst -> convert/SToC -> CAst -> c/CPrinter -> b.c
         (s/vendor: Lexer, Parser, Check)
```

**There is no third IR.** `CAst` and `SAst` are the two, and there is no pass
between them either: `CToS` and `SToC` each walk their input tree once and
build the other, lowering as they go. `lowerSwitch`, `lowerCountingFor`,
`lowerPrintf`, `hoistDeclarations` and `rename` are members of `CToS`, and a
`do-while` is peeled straight into Shalimar blocks — none of them is a
`CAst -> CAst` rewrite, and there is no intermediate C form to print.

`docs/ANALYSIS.md` §10 proposed one, under the name `Normalise`, and §12
milestone 5 scheduled it. **It was never built, and nothing is called that.**
The argument for it still stands and is worth reading before the file grows
much further — a separate pass could be tested on its own, and printed back
out as C to be read, neither of which is true of a lowering buried in a
1,500-line visitor. Treat it as an open design question rather than as a
description of this tree.

## Two Shalimar layout rules the printer must not break

Both are enforced by `shc`'s parser, and both fail silently if a printer
reflows lines:

1. `?` and `??` must be the **first token on their line**, and their item list
   runs to the end of that line. Every print statement gets a line to itself.
2. A `return`'s expression must be on the **`return`'s own line**.

## Diagnostics

Every refusal carries `{file, line, column, code, message, source line, hint}`
and goes in `Diagnostics`. Nothing calls `exit`, and nothing stops at the first
error — one pass over a file should be enough to learn everything wrong with
it. When any error is outstanding, **no output file is written**: a converter
that half-writes a file is worse than one that refuses.

## Verification

Prove the artefact, not the exit status. The suite for each case compiles the
input with its owning compiler, converts it, compiles the output with the
other, runs both, and diffs what they printed. A green run with no oracle
present proves nothing — `make test` says which oracles it found.
