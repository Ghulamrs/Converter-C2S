# Converter-C2S

A source-to-source converter between **C89** and **Shalimar**, in both
directions, built to the syntax rules that `Compiler-C` (`cc1`) and
`Compiler-S` (`shc`) define.

```
c2s prime.c            # C89 -> Shalimar, to standard output
c2s prime.shm -o p.c   # Shalimar -> C89
```

ISO C++14, built at `-Wall -Wextra -Werror -pedantic`, the same discipline the
two compilers are built under.

---

## What it does not do

**Headers are not converted.** A C source is preprocessed so that its
declarations resolve — `cc1` refuses an undeclared name, so `sqrt` will not
parse without `<math.h>` — but nothing that came from a header is translated.
Only what the named file itself contributed reaches the output.

**A preprocessor construct that changes the program is reported, not
translated.** `#include` is accepted and dropped. `#define`, `#if`, `#ifdef`,
`#pragma` and the rest are each reported with a file and a line, and the run
stops. They are answered by hand before conversion starts, because until they
are, what the parser saw is not what the author wrote.

**A construct with no expression in the target language is a conversion
error**, with the line, the column, the source quoted back and what to write
instead. It is never guessed at, and no output file is written when one is
outstanding. `docs/ANALYSIS.md` §6.3 is the catalogue.

**A round trip is not the identity.** C → Shalimar → C does not return the
original file: declarations move to the top of their function, `switch` is
gone, the type set narrows to Shalimar's three scalars, and the layout is the
converter's. What is preserved is what the program *does*, and the test suite
checks exactly that by compiling and running both sides.

---

## The two directions are not mirror images

C89 is a general systems language; Shalimar is a small whole-program numeric
language with three scalar types, no pointers, no aggregate but the array, no
preprocessor and twenty builtins. So:

- **C89 → Shalimar is mostly a rejection problem.** Most of C has no target.
  The work is deciding precisely what cannot be said, saying where, and
  lowering the handful of things that can be rewritten — `switch` into
  `if`/`elseif`/`else`, `do`-`while` into `while`, `?:` into a branch and a
  temporary, block-scoped declarations to the top of the function.
- **Shalimar → C89 is mostly a runtime problem.** Nearly every construct has a
  C89 spelling, but arrays carry their own extents, `char[]` is text with `+`
  and comparison, and `?` prints — and C89 has none of those. Extents travel
  as extra parameters; the rest is generated C.

---

## Layout

```
src/            the converter
  Source        one file, held whole, with the line index a diagnostic needs
  Diagnostics   every message a run produced; nothing here stops a run
  Options       the command line
  c/            the C89 front end: lexer, AST, parser, printer
  s/            the Shalimar front end: lexer, AST, parser, printer
  convert/      the lowering passes and the two mappings
tests/          the differential suite
docs/           ANALYSIS.md - why this is built the way it is
```

Nothing here compiles or links Compiler-C's or Compiler-S's sources, and
neither repository is modified. They are used as **oracles**: the test suite
compiles every input with its owning compiler and every output with the other,
runs both, and compares what they printed. That is what keeps two transcribed
grammars honest.

---

## Building

```
make              build ./c2s.exe
make test         build, then run the differential suite
make clean
```

`make test` looks for `../Compiler-C/cc1.exe` and `../Compiler-S/shc.exe`;
override with `CC1=` and `SHC=`.
