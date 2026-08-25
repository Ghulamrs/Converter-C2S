# Converter-C2S — analysis, and the issues in the way

Written before any code, from a full read of `Compiler-C/src` and `Compiler-S/src`.
Nothing here is a guess: every claim names the file it came from.

Repository: `git@github.com:Ghulamrs/Converter-C2S.git`
Working tree: `~/Documents/Claude/Converter-C2S`
Standard: ISO C++14, to match both compilers.

---

## 0. What I read

| Read in full | Why |
| --- | --- |
| `Compiler-C/src/{Ast,Type,Parser,Lexer,Preprocessor,Source,Driver}.h`, `Parser.cpp`, `Driver.cpp`, `Makefile`, `CLAUDE.md`, `README.md` | the C89 front end I would be reusing |
| `Compiler-S/src/{Ast,Type,Token,Parser,Check,Builtin,Diag,Driver}.h`, `Parser.cpp`, `Lexer.cpp`, `Check.cpp`, `Makefile`, `CLAUDE.md`, `README.md`, `docs/CONFORMANCE.md` | the Shalimar front end, and the grammar I must emit |
| `Compiler-S/examples/*.shm` | idiomatic Shalimar, and the shape a converter should aim to produce |
| `Shalimar/SHALIMAR_LANGUAGE.md` | located; Compiler-S defers to it as the specification |

---

## 1. The shape of the problem

The two languages are not siblings. C89 is a general systems language; Shalimar is a
small whole-program numeric language with three scalar types, no pointers, no
aggregates but the array, no preprocessor, and twenty builtins.

That asymmetry decides the character of each direction:

- **C89 → Shalimar is mostly a rejection problem.** Most of C has no target. The
  converter's main job is to decide precisely *what* cannot be expressed and say so
  with a location, and to lower the handful of constructs that can be rewritten.
- **Shalimar → C89 is mostly a runtime problem.** Almost every Shalimar construct has
  a C89 spelling, but three features — arrays that carry their own extents, `char[]`
  as text, and `?` printing — need C support code that does not exist in C89 itself.

Neither direction is symmetric with the other, and neither is a round trip
(see §9).

---

## 2. Issue #1 — Compiler-C's AST is a code-generation tree, not a source tree

This is the largest issue and it contradicts the proposal in the brief
("create AST from C source using Compiler-C first and then go back from AST to
syntax using Compiler-S AST"). The proposal is right in outline and wrong in one
assumption: that `Compiler-C`'s AST still resembles the source.

`Compiler-C/src/Parser.cpp` desugars while it parses, and `Ast.h` has no
declaration nodes at all. Verified in the sources:

| In the C source | In the AST |
| --- | --- |
| `int x;` | **nothing** — `Parser::declarationBody()` returns `StmtPtr(new Block({}))` |
| `int x = 3;` | a scope-less `Block` holding one `Assign` |
| `char s[32] = "hi";` | a `Block` of element-wise stores / fills |
| `typedef …`, block-scope `extern`, `;` | an empty `Block` |
| `enum E { A, B };`, `A` | erased; `enumSpecifier()` ends `return types_.intType();`, `A` becomes `Num(0)` |
| `sizeof(T)` | folded to `Num` at parse time |
| `a[i]` | `Unary('*', Binary(Add, a, i*scale))` — **no subscript node** |
| `p->m` | `MemberAccess(Unary('*', p), …)` — indistinguishable from `.` |
| `~x` | `Binary(BitXor, x, Num(-1))` |
| `++x`, `x += e` | `Assign(x, …)`, sometimes via a synthetic `Var::local("$compound", slot)` that appears in no declaration |
| an implicit promotion | a `Cast` node, **with no flag** distinguishing it from a written `(int)x` |
| a global initialiser | `std::vector<GlobalPiece>` — offset/size/`long long` bit images |

Also: `Expr` carries **no source position** (only `Stmt::pos()` does), and the
positions that exist are byte offsets into the *preprocessed* text, resolvable only
through `Source::locate` while the `Source` is still alive.

**What survives, and helps.** `Function::locals()` keeps every local's
`{name, type, offset, isParam, staticName, scope}` in declaration order, and
`Function::blocks()` gives the scope tree. That is exactly what Shalimar needs,
because Shalimar refuses a declaration below the top of a function body
(`Check.cpp`: `"'x': declare it at the top of the function"`). And several of the
lowerings are *useful* for this target — `sizeof` and `enum` folding, explicit
conversion nodes, `++x` already an assignment — because Shalimar has none of those
features anyway.

**What hurts.** Re-sugaring `*(a + i*8)` back to `a[i]`, and reconstructing
`char s[32] : "hello"` from thirty-two stores, are pattern-matching exercises whose
failure mode is silently ugly output. And a *written* `(double)x` cannot be told from
a promotion, so cast diagnostics would be noisy.

**The decisive argument, though, is the other direction.** Shalimar → C89 has to
*construct* C and print it. Compiler-C's AST cannot be hand-constructed for that
purpose — there are no declaration nodes to build, and `Var::offset()`,
`Function::frameSize()`, `Call::resultSlot()` and `sretSlot()` are ABI-computed frame
numbers — and there is no C printer anywhere in the tree. **So Converter-C2S needs
its own source-faithful C89 AST and its own C printer regardless of what the C→S
direction does.** Once that exists, having the C→S direction parse *into the same
AST* costs one recursive-descent parser and removes every re-sugaring problem above.

See §10 for the architecture this leads to, and Decision 1 for the choice.

---

## 3. Issue #2 — a C syntax error would kill the converter process

`Compiler-C/src/Source.h`:

```cpp
[[noreturn]] void fail(std::size_t pos, const std::string &message) const;
```

`Driver.cpp` says twice, in comments, that *"every diagnostic path in the compiler
reaches `std::exit`"*. `Parser.cpp` calls `fail` around two hundred times. There is
no diagnostic class, no collection, **no warnings at all**, and no recovery: the
first bad token in the input ends the process, with a message on stderr that the
caller never sees as data.

Two further hard stops of the same kind:

- `Parser::parse()` ends with `if (program.functions.empty()) src_.fail(0, "the file defines no functions");`
- Compiler-C refuses K&R definitions and refuses an undeclared name rather than
  assuming `int` — real-world C89 will hit both.

If Converter-C2S links Compiler-C's parser, it inherits all of this. The fix is a
one-line change in `Source::fail` — throw instead of exit, which is valid at every
existing call site because the function is already `[[noreturn]]` and the
`unique_ptr` tree unwinds cleanly — but that is a change to *your* repository.
See Decision 2.

---

## 4. Issue #3 — neither compiler builds a library

`Compiler-C/Makefile` builds only `cc1.exe`; `Compiler-S/Makefile` builds only
`shc.exe` plus the Shalimar *runtime* archives (`shmrt-*.a`, which are for compiled
Shalimar programs, not for the front end). There is no `libcc1front.a`, no
`libshc.a`, no install step — the README states *"nothing installs this compiler; it
runs from the tree it was built in."*

The minimum front-end sets, verified by include-graph:

```
Compiler-C:  src/Source.cpp  src/Preprocessor.cpp  src/Lexer.cpp  src/Parser.cpp  src/Type.cpp
             + a Target subclass (6 pure virtuals; the real ones live in src/backend/)
Compiler-S:  src/Type.cpp  src/Ast.cpp  src/Lexer.cpp  src/Parser.cpp  src/Check.cpp
             src/Builtin.cpp  src/Diag.cpp
```

Neither set touches `backend/`. Both trees compile at
`-std=c++14 -Wall -Wextra -Werror -pedantic`, and Compiler-C also bakes in
`-DCC1_INCLUDE_DIR` pointing at its `lib/` — and it is the **`Driver`**, not the
`Preprocessor`, that pushes that onto the include search path
(`Driver.cpp:407`). Bypass the driver and `#include <stdio.h>` resolves to nothing
unless the converter pushes it itself.

Two more lifetime traps for a long-running process: `TypeTable` must outlive the
`Program` (every `const Type *` in the tree points into it) and **`TypeTable` leaks**
— `derived_` is `std::vector<Type *>` of bare `new` with an implicit destructor.
Shalimar's side leaks by design too (`README.md`: *"Nothing is freed."*).

Either the two Makefiles gain a library target, or Converter-C2S compiles the needed
`.cpp` files itself from `../Compiler-C/src` and `../Compiler-S/src` with its own
rules and never modifies them. See Decision 2.

---

## 5. Issue #4 — the "forget header files" rule collides with the preprocessor

The brief says headers are not copied or converted, and that a critical preprocessor
construct may be reported for fixing before conversion starts. Both are right, but
they interact:

- Compiler-C **rejects an undeclared name**. So `sqrt(x)` will not parse unless
  `<math.h>` was actually included and preprocessed. We therefore have to *run* the
  preprocessor to parse at all — we just must not *translate* what came out of the
  headers.
- The clean rule: preprocess normally, then translate only what the main file
  contributed. `Stmt::pos()` → `Source::locate(pos).file`, and `files()[0]` is the
  translation unit's own name — so `place.file == 0` is exactly "written in the file
  the user handed us". Everything from a header is parsed, used for type and
  declaration information, and then dropped.
- The "fix this first" report therefore has to be a **scan of the raw text before
  preprocessing**, because after preprocessing every directive is gone. Proposed
  policy, which is Decision 3: `#include` is accepted and silently dropped from the
  output; `#define`, `#undef`, `#if`, `#ifdef`, `#ifndef`, `#elif`, `#else`, `#endif`,
  `#line`, `#pragma` and `#error` are each reported with file and line as
  *fix-before-conversion*, and the run stops.

Note also that Shalimar has **no block comments** — only `//`. Every `/* … */` in a C
source has to become one or more `//` lines if comments are carried across at all
(Decision 5).

---

## 6. C89 → Shalimar: what maps, what lowers, what must error

### 6.1 Direct

| C89 | Shalimar |
| --- | --- |
| `int` | `int` (32-bit, but see the overflow note below) |
| `double`, `float` | `real` (64-bit — `float` silently widens) |
| `char` | `char` — but **isolated from arithmetic**, see 6.3 |
| `T a[N]`, `T a[N][M]` | `T a[N]` / `T a[N][M]`, declared at the top of the function |
| `f(a, b)` | `f(a, b)` |
| `if / else` | `if / elseif / else` — note **`elseif` is one keyword**; `else if` is a parse error |
| `while (c) { }` | `while c { }` |
| `for (i = A; i <= B; i += K)` | `for i : A to B step K` — **inclusive**, constant stride only |
| `return e;` | `return e` — expression must stay on the `return`'s line |
| `break`, `continue` | same — but `continue` in a Shalimar `for` still advances the counter |
| `+ - * / %` and the six comparisons | same, except `==` is written `=` |
| `printf`-free output | `?` / `??`, which must start their line |

### 6.2 Lowerable, with a rewrite

| C89 | Rewrite | Note |
| --- | --- | --- |
| `switch` | `if` / `elseif` / `else` | you approved this; but see §8 — fall-through and `break` are the catch |
| `do { } while (c);` | peeled first iteration, or a flag + `while` | doubles the body or adds a variable |
| `a ? b : c` | an `if`/`else` writing a temporary declared at the top of the function | ternary in an argument position needs statement lifting |
| `x++`, `x += e` | `x +: 1`, `x +: e` | Shalimar has **only** `+:` and `-:`; `*=`, `/=`, `%=` become `x : x * e` |
| `(double)x`, `(int)x` | `real(x)`, `int(x)` | `int()` **fails outside int range** where C truncates |
| `'a'`, `'\n'` | `char(97)`, `char(10)` | Shalimar has no character literals |
| `"a\nb"` | split across two `?` statements | Shalimar has **no string escapes at all** |
| `0x1F`, `010`, `1U`, `1L`, `1.0f` | decimal, unsuffixed | no hex, no octal, no suffixes |
| block-scoped locals | hoisted to a top-of-function `Declare`, α-renamed on shadowing | `Function::blocks()` gives the scope tree to do this correctly |
| `int a, b, c;` | three declarations | one name per declaration |
| `/* … */` | `// …` | |

### 6.3 Must raise a conversion error

Grouped as the converter reports them. Unlike the rest of this document, this
section is **maintained against the code** — README points at it as the
catalogue — and was corrected on 2026-08-25, where what was built turned out
to differ from what was proposed. `tests/cases/beyond/` holds a case per group.

**No representation at all** — pointers (`T *`, `&x`, `*p`, pointer arithmetic,
`NULL`), `struct`, `union`, `enum` as a *type*, `typedef`, `void *`, function
pointers, `goto` and labels, `extern`, a `static` **inside a function**,
`unsigned`/`short`/`long`/`long long`, `long double`, multi-dimensional `char`
(`char[][]` is refused — *"strings are 1-D"*), arrays of strings, designated
initialisers, zero-length arrays.

**Absent operators** — `&`, `|`, `^`, `~`, `<<`, `>>` (bitwise: in Shalimar `&` and
`|` are *logical*, `^` is *power*, `~`/`<<`/`>>` are not tokens), the comma
operator, `sizeof`, and an assignment used as a **value** inside a larger
expression (`while ((c = f()) != 0)`).

**Accepted, and dropped rather than refused.** These were listed above as
refusals and are not, because none of them changes what the program does in a
language with one translation unit and three scalars:

- `register` and `auto` — hints with no effect on meaning in C89.
- `signed int` — the same type as `int`.
- A **file-scope** `static` — it says "not visible to other translation units",
  and a Shalimar program is whole, so there are none. A `static` inside a
  function is a different matter and is refused: it would keep its value
  between calls, which is meaning, and there is nowhere to keep it.
- `const` and `volatile` — dropped. `const` costs a compile-time guarantee
  only. **`volatile` is the one that loses something real**, and it is dropped
  silently; nothing in this converter is looking at hardware, but the day
  something is, this is where it will have gone wrong.

**Lowered rather than refused.** All three were listed above as conversion
errors. The converter does better, and each is exercised by
`tests/cases/beyond/operators.c` so it stays that way:

- `!x` becomes `x = 0`, which is exact — `:` is assignment in Shalimar and `=`
  is the equality test.
- Unary `+x` is the identity, so it becomes `x`.
- `a = b = c` as a whole statement lifts into two statements in order. Only an
  assignment reaching an enclosing *expression* is refused.

**Variadic functions** are refused, but not as variadic: a call to one is
turned down for being a name neither defined in the file nor one of the twenty
builtins. A *definition* never gets that far — it needs `va_list`, a typedef
from a header this converter drops unread, so the parser meets two identifiers
and reports a syntax error. The "headers are not converted" rule holds for
calls, which C89 lets you read without a prototype; it does not hold for names
a header would have introduced as **types**. See `tests/cases/beyond/variadic.c`.

**Absent library** — the whole of `<stdio.h>`, `<stdlib.h>`, `<string.h>`,
`<ctype.h>`, `<time.h>`, `<assert.h>`, `errno`, `malloc`/`free`. `<math.h>` is the one
header with coverage, through 20 builtins plus `pi` and `e`. There is **no input
facility in Shalimar at all**, so any program that reads has no translation.

**Structural** — `main(int argc, char **argv)` (Shalimar's `main` takes nothing),
returning a struct or an array (outputs are scalars only; use an array
out-parameter, or multiple outputs and `<a,b> : f(…)`), separate compilation.

### 6.4 Four semantic traps that are not syntax

These are the ones that will produce a *compiling but wrong* translation if handled
naively, and each needs a policy (Decision 4):

1. **`&&` and `||` do not short-circuit in Shalimar.** `Check.cpp`/README: *"both
   sides are evaluated before either is asked."* `i < n && a[i] == 0` becomes an
   out-of-range index. A faithful translation is nested `if`s, which is impossible in
   expression position without lifting to statements and a temporary.
2. **`char` does no arithmetic.** `c - '0'` must become `int(c) - 48`; `+ - * / % ^`
   on a `char` is a check error.
3. **Integer overflow is a runtime error in Shalimar, not wraparound.** README:
   *"passing an int limit is an error here, never a wrapped value that looks right."*
   C code that relies on defined `unsigned` wraparound has no translation, and C code
   that merely overflows quietly will now abort.
4. **Names created by assignment inside a block die with the block.** The safe rule
   is never to rely on creation-by-assignment: emit a top-of-function `Declare` for
   every C local. Also, Shalimar keywords are matched **case-insensitively**, so a C
   identifier named `Step`, `To`, `Fun` or `Return` must be renamed.

---

## 7. Shalimar → C89: the four hard parts

The grammar maps back easily; the semantics do not.

1. **Arrays carry their extents; C arrays do not.** `.row`, `.col` and `.dim(n)` are
   runtime queries, extents may be runtime expressions (`real w[len(s)]` is legal —
   a VLA, which C89 does not have), and *"a row is a value that can be replaced"*, so
   a rank-2 array is an array of independently replaceable rows. Three options:
   pass extents as extra C parameters; emit a descriptor `struct`; or restrict to
   constant extents and error on the rest. This is the single biggest design choice
   in this direction (Decision 6).
2. **`char[]` is text with `+` (join) and the six comparisons.** In C89 those become
   `strcat`/`strcmp` into caller-owned buffers, with the buffer sizing done by the
   converter. `t : s` **copies** (`docs/CONFORMANCE.md` §1 — the compiler copies even
   though the app's interpreter aliases).
3. **`?` / `??` and `prec(n)`.** Items are space-separated, `?` appends a newline and
   *always leaves a trailing space before it*, and `prec(n)` changes real precision
   from that point on including the rest of its own line. Reproducing that byte-exactly
   in C means generating `printf` with derived formats, or a small emitted helper.
4. **Multiple outputs.** `fun <int,int> = prime(n: int)` and `<d,k> : prime(j)` become
   a C function with out-parameters — mechanical, but it changes every call site.

Plus: Shalimar's `^` is **power and right-associative**, and unary minus binds
*tighter* than `^` (`-2^2` is `4`). Every emitted C expression has to be
re-parenthesised from Shalimar's precedence tiers (`|` < `&` < comparisons < `+ -` <
`* / %` < `^` < unary `-` < postfix), which are not C's. And `pow()` needs
`<math.h>` — which raises Decision 5, since the brief says to forget headers.

---

## 8. `switch` → `if` / `elseif` / `else`: two catches

You approved the lowering, and it is the right one. Two things it does not cover:

- **Fall-through.** In Compiler-C's AST a `Case` *owns* the statement it labels and
  fall-through is expressed by the labelled statement simply being followed by the
  next one in the enclosing `Block`. Reproducing that as an `if` chain requires
  either duplicating the shared tail into every falling case, or a `matched` flag
  variable and a chain of `if matched | cond` tests. Empty grouped cases
  (`case 1: case 2:`) are the harmless majority and need neither.
- **`break` inside `switch` is not a loop `break`.** Shalimar's `break` is loop-only
  and is a *parse* error outside a loop. A `switch` inside a `while` whose cases end
  in `break` must have those breaks **deleted**, not translated — translating them
  would silently break the enclosing loop instead.

My proposal: support empty grouped cases and `break`-terminated cases with no
fall-through; raise a conversion error on genuine fall-through unless you want the
flag-variable form (Decision 4).

---

## 9. Round-tripping is not identity

C → Shalimar → C will not return your original file, and should not be expected to.
Declarations are re-ordered to the top of each function, `switch` is gone, types are
narrowed to the three Shalimar scalars, and comments and formatting are the
converter's. The contract worth committing to is **semantic preservation on the
translatable subset**, verified by compiling both sides and comparing program output
— not textual round-trip. Both compilers make that easy: `cc1.exe` and `shc.exe`
compile and run, and `Compiler-S/examples/*.shm` carry their expected output inline.

---

## 10. Proposed architecture

> **Note added 2026-08-25, after the code existed.** This section is a proposal
> and two parts of it were not taken. The preprocessor and the C lexer are this
> converter's own — `c/CPreScan` and `c/CLexer` — rather than Compiler-C's, and
> `c2s::Normalise` **was never built**: `CToS` walks the C tree once and lowers
> as it goes. `CLAUDE.md` describes the tree as it stands. What follows is what
> was proposed before there was one, kept because the argument for a separate
> pass is still the best statement of why one might be worth having.

Given §2, the shape falls out:

```
             ┌──────────────────── Converter-C2S ────────────────────┐
  a.c  ──►  cc1 Preprocessor ──► cc1 Lexer ──► c2s::CParser ──► CAst │
            (from Compiler-C)     (from Compiler-C)   (ours)         │
                                                          │          │
                                              c2s::Normalise (ours)  │  switch→if, do-while→while,
                                                          │          │  ternary→if, ++→+:, hoist decls,
                                                          ▼          │  α-rename, short-circuit→nested if
                                                    c2s::CtoS        │
                                                          │          │
                                                          ▼          │
                                    shalimar::Program ──► c2s::SPrinter ──►  a.shm
                                                                     │
  b.shm ──► shc tokenize ──► shc Parser ──► shc Checker ──► Program ──┼──► c2s::StoC ──► CAst ──► c2s::CPrinter ──► b.c
            (from Compiler-S, used whole — its AST is source-faithful)│
             └─────────────────────────────────────────────────────┘
```

Points worth stating explicitly:

- **No third bridge IR.** `CAst` and `shalimar::Program` *are* the two IRs. What the
  bridge-IR idea was really buying — writing the lowering rules once — is bought
  instead by `c2s::Normalise`, which rewrites `CAst` into the C89 subset that maps
  1:1 onto Shalimar. Every lowering is then a `CAst → CAst` pass that can be unit
  tested on its own and printed back out as C to be inspected.
- **Compiler-S is reused whole** (lexer, parser, checker). Its AST is source-faithful,
  its `Checker` resolves symbols and types, and building a Shalimar `Program` by hand
  and running `Checker::check` on it validates our output before it is ever printed.
- **The Shalimar printer must respect two layout rules** or it will silently change
  the program: `?`/`??` must be the first token on their line and swallow the rest of
  it, and a `return`'s expression must be on the `return`'s own line.
- **One conversion-error type, one report.** Every refusal in §6.3 carries
  `{file, line, column, construct, source snippet, why}`.
- **Verification** is a `tests/` suite that, for each case, compiles the input with
  the owning compiler, converts, compiles the output with the other compiler, runs
  both, and diffs stdout.

A caution I owe you on `c2s::CParser`: it is a second C89 grammar in your world, and
a second grammar drifts. The mitigation is that every converter test also feeds its
input to `cc1.exe`, so anything our parser accepts that Compiler-C rejects (or vice
versa) fails the suite.

---

## 11. Decisions I need from you

1. **C front end.** (a) Our own source-faithful C89 parser over Compiler-C's lexer
   and types — my recommendation, for the reasons in §2; (b) reuse Compiler-C's
   `Parser` and re-sugar the lowered tree; (c) change Compiler-C itself to build a
   source-level AST alongside its lowered one.
2. **May I modify Compiler-C and Compiler-S?** Specifically: a `lib` target in each
   Makefile, and `Source::fail` throwing rather than exiting (§3). If not, the
   converter compiles their sources itself and the exit-on-error problem needs
   another answer (out-of-process front end, or accepting that a malformed C file
   ends the run with cc1's own message).
3. **Preprocessor policy** — confirm §5: `#include` dropped silently, every other
   directive reported as fix-before-conversion with file and line, run stops.
4. **Strict or pragmatic** on the four traps in §6.4 and on `switch` fall-through
   (§8). Strict = a conversion error whenever the meaning cannot be preserved
   exactly. Pragmatic = rewrite `&&`/`||` into nested `if`s with lifted temporaries,
   materialise fall-through with a flag, and insert `int()` around `char`
   arithmetic — more code translated, larger diffs from the original.
5. **Generated C and headers.** The brief says forget header files. But emitted C
   that calls `pow`, `strcmp` or `printf` needs `<math.h>`, `<string.h>`,
   `<stdio.h>` to compile under `cc1`. May generated `.c` files carry the `#include`
   lines they need (my recommendation), or must they be header-free?
6. **Shalimar → C89 array extents** (§7.1): extra C parameters, a descriptor struct,
   or constant extents only with an error on the rest?
7. **Scope of the first milestone.** I would target the subset that
   `Compiler-S/examples/*.shm` and `Compiler-C/examples/ex01`–`ex03` exercise, both
   directions, with the full error catalogue in place — rather than trying for
   breadth first.

---

## 12. Proposed milestones

| # | Deliverable |
| --- | --- |
| 0 | Repo skeleton, Makefile (`-std=c++14 -Wall -Wextra -Werror -pedantic`), MSVC project, test harness that drives `cc1.exe` and `shc.exe` |
| 1 | `CAst` + `CPrinter`, round-tripping C89 text through parse → print |
| 2 | Shalimar → C89 end to end for scalar arithmetic, `if`, `while`, `for`, calls, `?` |
| 3 | Shalimar → C89 for arrays and `char[]`, per Decision 6 |
| 4 | `c2s::CParser` (or the chosen alternative from Decision 1) |
| 5 | `Normalise` — declaration hoisting, α-renaming, `switch`, `do-while`, ternary, compound assignment *(delivered, but not as a pass: these lowerings live inside `CToS` — see the note on §10)* |
| 6 | C89 → Shalimar end to end, with the full §6.3 error catalogue and locations |
| 7 | Differential test suite: compile-run-diff both directions on every example |

---

*Written after the fact: every milestone above is delivered and on `main`, with one
omission and one departure. The omission is the MSVC project in milestone 0 — there is
none, and the tree is built by the Makefile alone. The departure is milestone 5, which
arrived as lowerings inside `CToS` rather than as the `Normalise` pass §12 named; §10's
argument for separating it is still unanswered. Milestone 7's suite is 35 cases and runs
green against the real `cc1.exe` and `shc.exe`.*
