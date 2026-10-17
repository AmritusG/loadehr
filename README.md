# LoadEhr

FFGL plugins for Resolume Arena that render 3D models (.fbx, .obj, .gltf, .glb) loaded from files and morph between
up to four of them in a chain. Nothing is bundled: with no file chosen in a Model slot the plugin renders transparent.

Four bundles are built from one code base:

| Bundle | Type | ID |
|---|---|---|
| LoadEhr | source | `LEhr` |
| LoadEhr FX | effect (the incoming layer can be background, skin or reflection) | `LEfx` |
| LoadEhr Wow | source, with the volumetric / simulation looks | `LEwo` |
| LoadEhr Wow FX | effect, with the volumetric / simulation looks | `LEwf` |

## Install

Version 1.0.0. macOS 11 or later, Apple Silicon and Intel, Resolume Arena 7.x.

1. Download `LoadEhr-1.0.0.dmg` from [Releases](https://github.com/AmritusG/loadehr/releases) (signed and notarized by Apple).
2. Quit Arena, open the DMG and drag the four bundles into `Documents/Resolume Arena/Extra Effects` in your home folder
   (or a folder you added under Arena > Preferences > Video; one folder only, so Arena does not find them twice).
3. Start Arena: the plugins show as LoadEhr, LoadEhr FX, LoadEhr Wow and LoadEhr Wow FX.

Uninstall: quit Arena and delete the four `.bundle` files from that folder.

## Build (macOS)

Needs CMake 3.20+ and Xcode command line tools. The bundles are universal (arm64 + x86_64), macOS 11+.

```
git clone --recurse-submodules https://github.com/AmritusG/loadehr.git
cd loadehr
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Install: copy `build/*.bundle` into `~/Documents/Resolume Arena/Extra Effects/` and restart Arena.
`CLAUDE.md` is the full working reference (parameters, architecture, test tools).

## Licence

MIT, see [LICENSE](LICENSE). Third-party code in `third_party/` keeps its own licence (MIT, BSD, zlib, ISC, Boost,
public domain); each folder carries its notice (`third_party/*/LICENSE*`, versions in `third_party/VERSIONS.txt`).
Test assets: [docs/ASSETS.md](docs/ASSETS.md).
