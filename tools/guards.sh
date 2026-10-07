#!/bin/sh
# LoadEhr hard-rule guards (part of tools/regress.sh). Exit 1 on any failure.
#  G1 no param named Library* or Laser* in any of the four bundles
#  G2 no code path that opens a directory listing for models: the only directory API in src/ is the texture lookup next to an
#     already chosen model file (ModelLoader.cpp FolderIndex: the model's own folder + its textures/ subfolder, textures only)
#  G3 no model file inside any built bundle
#  G4 nothing loaded = transparent: a fresh instance of each bundle covers 0 pixels; every Model slot is empty; Status A says so
#  G5 no laser / library source files, no library tools
cd "$(dirname "$0")/.."
fail=0
say() { printf '%s\n' "$*"; }
T=$(mktemp -d)
for b in LoadEhr LoadEhrFX LoadEhrWow LoadEhrWowFX; do
	printf 'bundle build/%s.bundle\nsize 320 180\ninstance\nparams\nframes 3\nmeans 2\nget Model A\nget Model B\nget Model C\nget Model D\nget Status A\n' "$b" > "$T/g_$b.txt"
	./build/loadehr_harness "$T/g_$b.txt" > "$T/g_$b.out" 2>/dev/null
	n=$(grep -c -E '^\s+[0-9]+ \|' "$T/g_$b.out")
	bad=$(grep -E '^\s+[0-9]+ \|' "$T/g_$b.out" | awk -F'|' '{print $3}' | grep -i -E '^\s*(library|laser)' | tr '\n' ',')
	if [ -n "$bad" ]; then say "G1 FAIL $b: $bad"; fail=1; else say "G1 ok   $b: $n params, none named Library* / Laser*"; fi
	px=$(grep '^means frame' "$T/g_$b.out" | sed -E 's/.*\| ([0-9]+) px$/\1/' | sort -u | tr '\n' ' ')
	empty=$(grep -c -E "^get Model [A-D] = ''$" "$T/g_$b.out")
	st=$(grep '^get Status A' "$T/g_$b.out")
	if [ "$px" = "0 " ] && [ "$empty" = 4 ] && echo "$st" | grep -q 'transparent'; then say "G4 ok   $b: covered pixels $px; 4 empty slots; $st"
	else say "G4 FAIL $b: covered '$px', empty slots $empty, $st"; fail=1; fi
done
hits=$(grep -n -E 'opendir|readdir|directory_iterator|contentsOfDirectory|enumeratorAtPath|scandir|glob\(|fts_open|NSFileManager|std::filesystem' src/*.cpp src/*.h src/*.mm src/*.inc)
if [ "$(echo "$hits" | grep -c .)" = 2 ] && [ "$(echo "$hits" | grep -c 'src/ModelLoader.cpp')" = 2 ] && grep -q 'struct FolderIndex' src/ModelLoader.cpp; then
	say "G2 ok   the only directory API: ModelLoader.cpp FolderIndex (textures next to the chosen model file):"; echo "$hits" | sed 's/^/        /'
else say "G2 FAIL directory APIs:"; echo "$hits"; fail=1; fi
models=$(find build/LoadEhr*.bundle -type f \( -iname '*.obj' -o -iname '*.fbx' -o -iname '*.gltf' -o -iname '*.glb' -o -iname '*.bin' -o -iname '*.mtl' -o -iname '*.png' -o -iname '*.jpg' \) | grep -v Probe)
if [ -z "$models" ]; then say "G3 ok   no model / texture file inside build/LoadEhr*.bundle ($(find build/LoadEhr.bundle build/LoadEhrFX.bundle build/LoadEhrWow.bundle build/LoadEhrWowFX.bundle -type f | wc -l | tr -d ' ') files: binaries + Info.plist)"
else say "G3 FAIL: $models"; fail=1; fi
gone=$(ls src/Laser* src/Library* tools/w19_* tools/prune.sh tools/w17_library.py 2>/dev/null)
if [ -z "$gone" ]; then say "G5 ok   no Laser* / Library* sources, no library tools"; else say "G5 FAIL: $gone"; fail=1; fi
rm -rf "$T"
exit $fail
