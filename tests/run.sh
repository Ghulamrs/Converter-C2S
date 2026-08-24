#!/bin/sh
# The differential suite: prove the artefact, not the exit status.
#
# Every case is compiled with its owning compiler, converted, compiled with
# the other compiler, run twice, and its outputs compared. The oracles are
# the real cc1.exe and shc.exe; a run without them proves nothing and says
# so rather than passing.
#
#   tests/cases/s2c/*.shm    Shalimar -> C89, byte-identical output
#   tests/cases/c2s/*.c      C89 -> Shalimar, identical after stripping the
#                            trailing space Shalimar's '?' always writes
#   tests/cases/beyond/*.c   must convert with markers and exit 1
#   tests/cases/defines/*.c  must stop with the preprocessor decision list

set -u
here="$(cd "$(dirname "$0")" && pwd)"
c2s="$here/../c2s.exe"
CC1="${CC1:-$here/../../Compiler-C/cc1.exe}"
SHC="${SHC:-$here/../../Compiler-S/shc.exe}"
out="$here/out"
mkdir -p "$out"

pass=0; fail=0

fails() { echo "FAIL: $1"; fail=$((fail+1)); }

if [ ! -x "$CC1" ]; then echo "no cc1 oracle at $CC1 - set CC1="; exit 2; fi
if [ ! -x "$SHC" ]; then echo "no shc oracle at $SHC - set SHC="; exit 2; fi

for f in "$here"/cases/s2c/*.shm; do
    n=$(basename "$f" .shm)
    "$SHC" "$f" -o "$out/s_$n" 2>"$out/e" || { fails "$n: shc refused the original"; continue; }
    "$c2s" "$f" -o "$out/conv_$n.c" 2>"$out/e" || { fails "$n: markers or refusal: $(head -1 "$out/e")"; continue; }
    "$CC1" "$out/conv_$n.c" -o "$out/c_$n" 2>"$out/e" || { fails "$n: cc1 refused the conversion: $(head -2 "$out/e")"; continue; }
    "$out/s_$n" > "$out/o1" 2>&1
    "$out/c_$n" > "$out/o2" 2>&1
    if cmp -s "$out/o1" "$out/o2"; then pass=$((pass+1)); else fails "$n: outputs differ"; fi
done

for f in "$here"/cases/c2s/*.c; do
    n=$(basename "$f" .c)
    "$CC1" "$f" -o "$out/c_$n" 2>"$out/e" || { fails "$n: cc1 refused the original"; continue; }
    "$c2s" "$f" -o "$out/conv_$n.shm" 2>"$out/e" || { fails "$n: markers or refusal: $(head -1 "$out/e")"; continue; }
    "$SHC" "$out/conv_$n.shm" -o "$out/s_$n" 2>"$out/e" || { fails "$n: shc refused the conversion: $(head -2 "$out/e")"; continue; }
    "$out/c_$n" > "$out/o1" 2>&1
    "$out/s_$n" 2>&1 | sed 's/ *$//' > "$out/o2"
    sed 's/ *$//' "$out/o1" > "$out/o1s"
    if cmp -s "$out/o1s" "$out/o2"; then pass=$((pass+1)); else fails "$n: outputs differ"; fi
done

for f in "$here"/cases/beyond/*.c; do
    n=$(basename "$f" .c)
    "$c2s" "$f" -o "$out/conv_$n.shm" 2>"$out/e"
    rc=$?
    if [ $rc -ne 1 ]; then fails "$n: expected exit 1 with markers, got $rc"; continue; fi
    if ! grep -q 'BEYOND SHALIMAR' "$out/conv_$n.shm"; then fails "$n: no markers in the output"; continue; fi
    pass=$((pass+1))
done

for f in "$here"/cases/defines/*.c; do
    n=$(basename "$f" .c)
    "$c2s" "$f" > "$out/o" 2>"$out/e"
    rc=$?
    if [ $rc -ne 1 ]; then fails "$n: expected exit 1 with the decision list, got $rc"; continue; fi
    if ! grep -q 'preprocessor construct' "$out/e"; then fails "$n: no decision list"; continue; fi
    if [ -s "$out/o" ]; then fails "$n: output was written before the decisions"; continue; fi
    pass=$((pass+1))
done

# The canonical identities, proved against the same oracles.
for f in "$here"/cases/s2c/*.shm; do
    n=$(basename "$f" .shm)
    "$c2s" --canon "$f" -o "$out/canon_$n.shm" 2>"$out/e" || { fails "canon $n: refused"; continue; }
    "$SHC" "$out/canon_$n.shm" -o "$out/cs_$n" 2>"$out/e" || { fails "canon $n: shc refused"; continue; }
    "$out/s_$n" > "$out/o1" 2>&1
    "$out/cs_$n" > "$out/o2" 2>&1
    if cmp -s "$out/o1" "$out/o2"; then pass=$((pass+1)); else fails "canon $n: outputs differ"; fi
done

echo
echo "pass=$pass fail=$fail"
[ $fail -eq 0 ]
