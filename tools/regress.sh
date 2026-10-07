#!/bin/sh
# usage: tools/regress.sh <verify subdir>     e.g. tools/regress.sh w2
#        tools/regress.sh --make              (once, W1) render the references into verify/w1/ref and pin them
# Renders the LoadEhr regression set by parameter name and diffs it against verify/w1/ref (every compare must read max 0),
# then runs the hard-rule guards (tools/guards.sh). The references are pinned by tools/regress_refs.sha256 (ref-hash guard).
set -e
cd "$(dirname "$0")/.."
[ -f test_models/gen/mask/mask.obj ] || python3 tools/gen_test_models.py >/dev/null
GEN="$(pwd)/test_models/gen"
if [ "$1" = "--make" ]; then
	mkdir -p verify/w1/ref
	sed -e "s|@OUT@|w1/ref|g" -e "s|@GEN@|$GEN|g" -e '/^compare /d' tools/scripts/regress.txt > verify/w1/ref/regress_script.txt
	./build/loadehr_harness verify/w1/ref/regress_script.txt > verify/w1/ref/regress.log
	chmod a-w verify/w1/ref/*.png
	shasum -a 256 verify/w1/ref/*.png > tools/regress_refs.sha256
	cat tools/regress_refs.sha256
	exit 0
fi
OUT="$1"
shasum -a 256 -c tools/regress_refs.sha256 >/dev/null || { echo "REGRESSION REFERENCES CHANGED: verify/w1/ref does not match tools/regress_refs.sha256"; exit 1; }
echo "ref-hash guard ok: $(wc -l < tools/regress_refs.sha256 | tr -d ' ') references match tools/regress_refs.sha256"
mkdir -p "verify/$OUT"
sed -e "s|@OUT@|$OUT|g" -e "s|@GEN@|$GEN|g" tools/scripts/regress.txt > "verify/$OUT/regress_script.txt"
./build/loadehr_harness "verify/$OUT/regress_script.txt" | tee "verify/$OUT/regress.log" | grep -E "^(compare|stats|unknown)|FAIL|TIMEOUT"
if grep '^compare' "verify/$OUT/regress.log" | grep -v ' max 0 ' ; then echo "REGRESSION: a render differs from its reference"; exit 1; fi
./tools/guards.sh
