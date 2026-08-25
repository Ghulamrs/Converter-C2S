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
#   tests/cases/allow/*.c    the same, but converted under the permission in
#                            the case's .flags file - the rewrites that are
#                            refused by default because each one compiles
#                            without meaning quite what the C did. Proving
#                            these needs a differential run and nothing less:
#                            a short-circuit rewrite that got the guard wrong
#                            still compiles, and only running it says so.
#   tests/cases/beyond/*.c   valid C89 that cc1 accepts, refused with
#                            markers and exit 1; every refusal counted
#                            must also be one shown, and any patterns in
#                            the case's .expect file must be among them
#   tests/cases/defines/*.c  must stop with the preprocessor decision list
#   the command line          exit statuses and codes, which no case above
#                            reaches - every one of them hands c2s a good
#                            file and a good option
#
# A case in any directory may carry a <name>.flags file. Its contents are
# passed to c2s and to nothing else. In beyond/ that is how a refusal is
# pinned as a refusal THE PERMISSION DOES NOT LIFT, which is a different
# statement from one that has never been asked about.

set -u
here="$(cd "$(dirname "$0")" && pwd)"
# All three are overridable, and for the same reason. A workspace build puts
# every binary in one directory rather than in each repository's own root, so
# the converter is no more reliably beside this script than the oracles are -
# and deriving it from $here alone meant a group build ran the whole suite
# against a c2s that was not there. That failed 59 of 59 rather than passing,
# which is the good version of the fault, but it is still a suite that could
# not run being reported as a suite that failed.
c2s="${C2S:-$here/../c2s.exe}"
CC1="${CC1:-$here/../../Compiler-C/cc1.exe}"
SHC="${SHC:-$here/../../Compiler-S/shc.exe}"
out="$here/out"
mkdir -p "$out"

pass=0; fail=0

fails() { echo "FAIL: $1"; fail=$((fail+1)); }

# The flags a case is converted under, or nothing. Read as words rather than
# quoted, so a .flags file holding two options works.
flagsfor() {
    if [ -f "$1" ]; then cat "$1"; fi
}

if [ ! -x "$c2s" ]; then echo "no c2s at $c2s - set C2S="; exit 2; fi
if [ ! -x "$CC1" ]; then echo "no cc1 oracle at $CC1 - set CC1="; exit 2; fi
if [ ! -x "$SHC" ]; then echo "no shc oracle at $SHC - set SHC="; exit 2; fi
echo "c2s $c2s"
echo "cc1 $CC1"
echo "shc $SHC"

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

# The permission cases. Same differential shape as c2s above - the only
# difference is the flags, and the fact that without them every one of these
# would be refused rather than wrong.
for f in "$here"/cases/allow/*.c; do
    [ -e "$f" ] || continue
    n=$(basename "$f" .c)
    flags=$(flagsfor "$here/cases/allow/$n.flags")

    # First: it must still be refused WITHOUT the permission. A case that
    # converts either way is not testing a permission, and would go on
    # passing if the flag stopped being read - which is the exact fault
    # this whole section exists because of.
    if "$c2s" "$f" -o "$out/bare_$n.shm" 2>"$out/e"; then
        fails "$n: converts without $flags, so it proves nothing about it"
        continue
    fi

    "$CC1" "$f" -o "$out/c_$n" 2>"$out/e" || { fails "$n: cc1 refused the original"; continue; }
    # shellcheck disable=SC2086
    "$c2s" "$f" -o "$out/conv_$n.shm" $flags 2>"$out/e" ||
        { fails "$n: markers or refusal under $flags: $(head -1 "$out/e")"; continue; }
    "$SHC" "$out/conv_$n.shm" -o "$out/s_$n" 2>"$out/e" ||
        { fails "$n: shc refused the conversion: $(head -2 "$out/e")"; continue; }
    "$out/c_$n" > "$out/o1" 2>&1
    "$out/s_$n" 2>&1 | sed 's/ *$//' > "$out/o2"
    sed 's/ *$//' "$out/o1" > "$out/o1s"
    if cmp -s "$out/o1s" "$out/o2"; then pass=$((pass+1)); else
        fails "$n: outputs differ under $flags"
        diff "$out/o1s" "$out/o2" | head -6 | sed 's/^/    /'
    fi
done

for f in "$here"/cases/beyond/*.c; do
    n=$(basename "$f" .c)

    # A rejection case has to be valid C89 first, or what it proves is that
    # the C parser choked - not that the mapping refused. cc1 is the
    # authority on that, and -c because a case is not required to link.
    "$CC1" -c "$f" -o "$out/b_$n.o" 2>"$out/e" ||
        { fails "$n: cc1 refused the case itself: $(head -2 "$out/e")"; continue; }

    flags=$(flagsfor "$here/cases/beyond/$n.flags")
    # shellcheck disable=SC2086
    "$c2s" "$f" -o "$out/conv_$n.shm" $flags 2>"$out/e"
    rc=$?
    if [ $rc -ne 1 ]; then fails "$n: expected exit 1 with markers, got $rc"; continue; fi
    if ! grep -q 'BEYOND SHALIMAR' "$out/conv_$n.shm"; then fails "$n: no markers in the output"; continue; fi

    # Every refusal counted must be a refusal shown. Three separate faults
    # have hidden in that gap - a printer that did not recognise the marker
    # node, a marker built with no block to go in, and a marker dropped on
    # the way out of the globals holder - and each one left behind a
    # smaller program that compiled and ran. The count is on stderr, the
    # markers are in the file, and they have to agree.
    printed=$(grep -c 'BEYOND SHALIMAR' "$out/conv_$n.shm")
    counted=$(sed -n 's/.*: \([0-9][0-9]*\) constructs* ha[sv]e* no expression.*/\1/p' \
              "$out/e" | head -1)
    if [ -z "$counted" ]; then fails "$n: the run did not say how many it refused"; continue; fi
    if [ "$printed" != "$counted" ]; then
        fails "$n: $counted refused, $printed marked - a refusal went missing"
        continue
    fi

    # Named refusals, where the case says which ones it is for. One grep
    # pattern per line - a basic regular expression, so a '*' in a C type
    # needs its backslash - and a case with no .expect asserts only the
    # above. Every miss is listed, not just the last one found.
    if [ -f "$here/cases/beyond/$n.expect" ]; then
        missing=0
        while IFS= read -r want; do
            [ -z "$want" ] && continue
            if ! grep -q "$want" "$out/conv_$n.shm"; then
                echo "    $n: nothing refused for: $want"
                missing=$((missing+1))
            fi
        done < "$here/cases/beyond/$n.expect"
        if [ "$missing" -ne 0 ]; then
            fails "$n: $missing expected refusal(s) missing"
            continue
        fi
    fi

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
