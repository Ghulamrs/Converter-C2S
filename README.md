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
The one thing a header leaves behind is the borrow: a converted program that
calls `sqrt` is emitted with `uses sqrt` at the top, because Shalimar has no
library function until a file asks for one.
Only what the named file itself contributed reaches the output.

**A preprocessor construct that decides *which program this is* is reported,
not translated.** `#include` is accepted and dropped. `#if`, `#ifdef`,
`#ifndef`, `#elif`, `#else`, `#endif` — and any `#define` one of those tests —
are each reported with a file and a line, and the run stops. `#undef`,
`#pragma`, `#error` and `#line` likewise. They are answered by hand before
conversion starts, because until they are, what the parser saw is not what the
author wrote:

```c
#define TEST_VERSION
#ifdef TEST_VERSION
float  test = 0.0f;
#else
double test = 0.0;
#endif
```

There are two programs there and nothing in the file says which is wanted.

**One conditional decides nothing, and is dropped rather than asked about.**

```c
#ifndef M_PI
#define M_PI 3.14
#endif
```

An `#ifndef NAME` holding nothing but the `#define NAME` it guards is not a
question: a file being converted has no other translation unit to have defined
the name first, so the definition always stood. The two lines go, the middle
one is the substitution it always was, and the Shalimar says in a comment that
they were dropped. Narrow on purpose — `#ifdef` means the opposite, an `#else`
is a choice between programs, a name that does not match the one being defined
is not a guard around it, and anything else at all inside the block makes it a
question again. Every one of those is in `tests/cases/defines/guardlike.c`.

Refusing this shape stopped conversions over a header idiom that appears in
almost every file wanting a constant — `M_PI` most of all, which MSVC hides
behind `_USE_MATH_DEFINES` and everybody therefore defines by hand.

**A `#define` that only names a value is expanded, not reported.** `#define PI
3.14159` does not branch anything — it names a number, and substituting it is
what the author meant. Object-like and function-like macros alike are
substituted into the token stream after lexing, so a diagnostic still quotes
the line as it was written rather than as it expanded. Only a replacement
using `#` or `##` goes back to the author, having no token-level equivalent.

**`printf` becomes a print list, and `%.Nf` becomes `prec(N)`.** They are the
same thing said in two languages — a fixed number of decimal places — so
`printf("five %.5f\n", v)` is `? "five" prec(5) v`, and `%f` alone is
`prec(6)`, C's default. A precision on anything else is refused: `%.3d` is
zero-padding and `%.3s` is a truncation, and neither is what `prec` means.

**And `?` writes a space after every item, which is the one difference this
converter makes.** The language has no way to suppress it, and nothing to build
text with either — no concatenation, no number-to-text builtin — so a line
cannot be assembled as a single item instead. `printf("value %d.\n", n)` can
only become `? "value" n "."`, which writes `value 5 .` where the C wrote
`value 5.`.

It converts and warns, naming the line. Refusing it stops a conversion over one
space, and a program that prints a space too many is still the program; what
must not happen is the difference going unsaid, which is what happened until
2026-08-27 — the suite compares byte for byte and had no case with punctuation
against a hole. `tests/cases/spacing/` is that case now: both programs are run
and compared with every space removed, so a wrong number or a lost value still
fails and only where the spaces fall is forgiven.

A space in the format pays for the one `?` adds, so `"max %d min %d"` and
`"plain %d and %d"` come out exactly, and only `"%d%d"`, `"(%d)"` and
`"value %d."` gain one.

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
preprocessor, and a set of library functions each of which a file must borrow
with `uses` before it may call it. So:

- **C89 → Shalimar is mostly a rejection problem.** Most of C has no target.
  The work is deciding precisely what cannot be said, saying where, and
  lowering the handful of things that can be rewritten — `switch` into
  `if`/`else if`/`else`, `do`-`while` into `while`, `?:` into a branch — over
  the target of an assignment, or over two returns — block-scoped
  declarations to the top of the function.
- **Shalimar → C89 is mostly a runtime problem.** Nearly every construct has a
  C89 spelling, but arrays carry their own extents, `char[]` is text with `+`
  and comparison, and `?` prints — and C89 has none of those. Extents travel
  as extra parameters; the rest is generated C.

**Shalimar's int traps are carried across.** Shalimar makes passing the int
limit a runtime error; C89 wraps and says nothing. So int `+`, `-` and `*`
become calls to helpers that check first and stop the same way — the same
message, on stdout, with the same exit status and the same line. It costs the
generated C some of its readability, which is the price of the two programs
answering alike. Reals are untouched, overflowing to infinity in both
languages; divide and modulus are not checked yet.

---

## Layout

```
src/            the converter
  Source        one file, held whole, with the line index a diagnostic needs
  Diagnostics   every message a run produced; nothing here stops a run
  Options       the command line
  c/            the C89 front end: lexer, AST, parser, printer
  s/            the Shalimar front end: lexer, AST, parser, printer
  convert/      the two mappings, each lowering as it walks
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
