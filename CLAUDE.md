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
a.c   -> c/CPreScan -> c/CLexer -> c/CMacro -> c/CParser -> CAst
                                                 |
                                    convert/CToS -> SAst -> s/SPrinter -> a.shm

b.shm -> s/SFrontEnd -> SAst -> convert/SToC -> CAst -> c/CPrinter -> b.c
         (s/vendor: Lexer, Parser, Check)
```

`c/CMacro` is the one pass that is not on the way to a tree: it substitutes
the `#define`s that name a value, on the **token stream** rather than on the
text. That placement is the whole point of it — see `docs/ANALYSIS.md` §5.
Rewriting the text would move every byte after the first replacement and
every diagnostic offset with it, so instead a token out of a replacement
carries the offset of the place the macro was *written*. A `#define` that a
conditional tests is not substituted at all; `CPreScan` sends that one back
to the author, because it decides which program this is.

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

## Comments in src/ are at most three lines

**The rule, as instructed across every repository of the product:** no
comment group in `src/` runs longer than three lines, and a group standing in
front of a *single* line of code runs one line only. The oracle is
`../C++Optimize/tools/comment-lines src`, with `--count` for a gate; it reads
`src/s/vendor` too, and that directory is Compiler-S's to cap - see below.

**The sweep, 2026-09-26: 43 groups in this tree's own sources, to 0.** Whole
sentences were kept front to back and never cut inside; a sentence that would
not fit its cap became one long line rather than a mangled short one; and the
paragraphs whose substance a reader would want back are recorded here, one
subsection each, so that the line beside the code can point at them. Git
history holds every long form. The honest residue: 81 comment lines in the
twelve edited files now run past 100 columns where none did before, which the
policy permits - it counts lines - and which is the price of not cutting inside
a sentence.

**`src/s/vendor` is not this repository's to edit.** It is a byte-for-byte
copy of `../Compiler-S/src`, mode 444 on purpose, and `make test`'s
`vendor-check` fails on any difference; a cap applied to the copy would be
either refused by that check or undone by the next `make vendor`. Its 19
groups over the cap are Compiler-S's, and `make vendor` brings them here once
that tree has been capped.

### What the long comments said

**The line map.** `Converter::Result::lineMap` and `SPrinter::lineMap()` give
the input line each output line came from, indexed by output line minus one,
and 0 where no construct owns it - the `uses` clause, the blank line between
two functions, anything a caller prepended. The numbers are the *input's*:
every statement in the tree carries the line of the construct it was built
from, and for a converted program that is a line of the C. It is filled for
C -> Shalimar only, because that is the direction with a reader who cannot see
the output: the app converts, hands the Shalimar to its interpreter, and
everything said from there names a line of a file that is not on screen.
`--canon` does not fill it, and neither does Shalimar -> C, where the C is
what the author is given. `--lines` prints it as `output: input` pairs on
standard error, `-` for an unowned line.

**The version number.** `kVersion` in `main.cpp` is numbered with the group
rather than on its own: c2s runs a particular cc1 and shc as oracles, so a
number that did not move with theirs would say less than nothing. 1.2 is the
release in which Shalimar gained `uses`, and this converter emits the clause
from then on. The banner has the family's shape - cc1, cxx1 and shc open the
same way - with what this one does where they name their language.

**`__LINE__` and `__FILE__`.** The two macros the preprocessor is required to
supply and the two Compiler-C supplies. Nothing defines them, so they are in
no table, and an identifier no table knows is emitted unchanged - which turned
a program that printed its own line number into one naming a variable that
does not exist; nothing here said so, and shc did afterwards about a file its
author never wrote. They are expanded on the token stream like every other
macro, the replacement keeping the offset it was written at, and `CMacro`
takes no early return for a file with no `#define`, since these two are used
without one.

**A dropped guard beside an include.** The guard shape README describes is
safe only if nothing else could have defined the name, and a header could:
`#ifndef M_PI / #define M_PI 3.14 / #endif` beside `#include <math.h>` is the
case that proves it, math.h defining M_PI as the full pi so that the C never
takes the 3.14 and a conversion that drops the guard does, and computes
different numbers. The converter translates nothing from a header and cannot
see what one defines, so `CPreScan` says so rather than pretending either way;
with no include in the file the drop is provable rather than likely. The
dropped guard is also said in the Shalimar itself, as a comment, since the
value is written out wherever the name was used - Shalimar has nowhere to put
a name outside a function.

**`else if` is one construct.** `CPrinter` writes a chain on one line per
branch, not `else` wrapping a nested `if`: the nested form indents a step per
branch, so the five-branch chain CToS writes as a flat list came back from
SToC indented six levels. It is walked as a loop rather than by recursing,
because recursing is what produced the indentation, and a chain of any length
costs no stack. Two shapes reach `chainedIf` and both are the same C - an
`else` arm that IS a CIf, and a compound holding one, which is what a Shalimar
`else` block whose only statement is an `if` lowers to. The arm is chained
**only when the block holds that `if` and nothing else**: `else { if (c) A B }`
runs B whatever c is and `else if (c) A B` does not, a single declaration is
the same trap wearing a type, so the count is checked before the kind and one
is the only count that qualifies. `SPrinter` spells the Shalimar side `else
if` for the same reason - the converted chain is the picture of the C it came
from - and there is no other spelling, Shalimar having dropped `elseif` on
2026-08-26.

**The library names.** `builtinFor` asks the vendored builtin table rather
than keeping a list: a hand-written copy drifted, knowing fourteen names where
Shalimar had twenty, so `hypot`, `round` and `trunc` converted to nothing while
being perfectly available, and would have missed the seven added on
2026-08-26 too. Shalimar borrowed the C names unchanged, so the mapping is
identity wherever the table has a row, with four exceptions, each a name that
means something different on the two sides:

| | |
| --- | --- |
| `fabs` | C spells the real one `fabs`; Shalimar's `abs` covers both |
| `max`, `min` | Shalimar's are `a > b ? a : b` and propagate NaN; C's nearest are fmax/fmin, which return the non-NaN operand - a different function, so one must not silently become the other (C89 has no bare max or min, so cc1 refuses them anyway) |
| `len` | an array's own, never C's |

`fmod(a, b)` becomes the `%` operator and borrows nothing; `BorrowScan` and
`CToS::visit(CCall &)` both decide that and must agree.

**The borrows come first.** `BorrowScan` answers which library names the file
will borrow before a single name has been renamed. Shalimar's `uses` is per
FILE and a C local is per function, so `sqrt()` called anywhere takes the name
away from every variable in the output - and the converting walk meets a
local named `sqrt` in one function long before, or long after, the call in
another. Renaming as we go got that wrong half the time, silently: valid C in,
invalid Shalimar out, refused by shc with "'sqrt' is borrowed on line 1". The
scan borrows `NameScan`'s traversal rather than writing a second one, thirty-
one visit methods copied being thirty-one chances to miss a node type. Only a
VARIABLE loses a borrowed name (Shalimar 7.5.1 rule 3); a program's own
function may share it and wins at the call, so `rename(name, false)` leaves a
function alone.

**Local names are per function.** Shalimar scopes a function's locals to that
function, so `x` in two functions is two variables and neither needs a new
name. `usedNames_` was one set for the whole unit, so the second `x` became
`x_2`, the third `x_2_2`, and a file of six small functions ended with
`x_2_2_2_2_2` - correct, and unreadable beside the C. The set is restored
rather than cleared on leaving a function: what it holds on entry is the file
scope - globals and every function's name - which locals must still avoid and
which the next function must still see.

**A falling switch.** A case running on into the next is lowered, not refused.
Unlike the other rewrites behind permissions this one changes nothing about
what the program means - the entry index and the done flag reproduce C's rule
exactly, default in the middle included. What it costs is the if/else-if
chain's readability, and only for a switch that falls through; that is a
price, not a risk, so it is not asked about. `switchFallsThrough` is asked
twice, by the hoist walk that mints the two temporaries and by the lowering,
and if the two disagree the switch is refused rather than left naming a
temporary nothing declared.

**Folding an opening assignment.** C says `double r = 0.0;` and Shalimar can
say `real r : 0.0`, but the converter said `real r` and then `r : 0.0` on the
next line. Not an oversight: declarations are hoisted to the top of the
function, because C89 puts them at the top of a block and Shalimar wants them
at the top of a function, and once a declaration moves its initialiser usually
cannot follow - inside a loop or one arm of an `if` it has to run where it was
written. So `foldOpeningAssignments` folds only an assignment that already
runs exactly once, unconditionally, at function entry, under three conditions:
it is one of the leading statements before anything branches or repeats; it
reads none of the locals not yet given a value, since folding moves it above
the statement that would have set them; and **folds keep their order among
themselves** - two initialisers that both move end up in declaration order,
so folding `b : f()` and then `a : g()` would run g() before f(), and only
ever folding into a later declaration than the last one folded keeps the two
orders the same.

**The spacing warning and `prec`.** README carries both rules; what the
comments added is that `spaceTaken` records whether a space in the format has
paid for the one `?` writes, `spacingSaid` says it once per printf rather than
once per character, and `kPrecisionLimit` is 17 because that is as far as a
double's decimal places go - C pads `%.30f` with digits that mean nothing, and
Shalimar is not asked to.

**Two printer layout facts.** A function's body gets a blank line between the
leading declarations and the code, because Shalimar gathers every declaration
at the top and without the gap the first real statement is just the next line
down; only the leading run counts. And the function's `{` goes on its own line
while `if`, `while` and `for` keep theirs on the condition's line - how the
language is written by hand, the head of a function being a thing you read on
its own. `sync()` banks the line map at the two edges of a statement rather
than at each of the twenty places a newline is written, because a hook at each
would be twenty chances to miss one.
