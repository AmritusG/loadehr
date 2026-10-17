project:    LoadEhr
pass:       w5-wire
STATUS:     COMPLETE
parent:     3f4987f
result:     bff6cc7
session:    3b26eb0d-b573-42d5-b913-073ed46d4d76
written_at: 2026-10-08T18:50:00+02:00
# LoadEhr W5 results: wireframe only, a wire colour, wire glow that blooms, in every bundle

## Amrit's answers this session
- Asked (verbatim, from the brief): "While that runs, can I have wireframe surface only? And settings to make the wireframe glow and blom?"
- Restarted Arena when asked (PID 25852 -> 43403). No other questions were needed.

## What shipped (commit bff6cc7, pushed; Commons a7efb40: LOADEHR-010)
- A new **"Wireframe" group in all four bundles**, placed after Deform. Wireframe Mix and Wire Thickness moved in from the classic FX
  group with their names kept, so saved values keep.
  - **Wire Mode:** Off / Over Surface / Wire Only / Wire + Dark Fill. The default is Over Surface, so old clips keep their W1 wire.
  - **Wireframe Mix** = the lines' strength in every mode. **At 0 there are no lines: Wire Only at Mix 0 shows nothing.**
  - **Wire Back:** Hidden / X-Ray.
  - **Wire Edges:** All Triangles / Feature Edges.
  - **Wire Color:** one Arena picker; its alpha "Wire Color Mix" = the picked colour vs the surface colour.
  - **Wire Glow**, **Wire Bloom** + **Wire Bloom Size**, **Wire Fade** (fades across the model, near side to far side).
- **Bundle table** (the 512 cap; both FX carriers 505 + 509 = 1014 <= 1024): **LoadEhr 501, FX 505, Wow 495, Wow FX 509.**
  Wireframe (13 params) is in every bundle. To make room in Wow FX, **Silhouette (8) moved Wow -> classic**, the way W2 moved Spots.
  Nothing was cut.
- **Zero cost when off:** Over Surface with every W5 dial at rest is the W1 path on the PLAIN programs (regress r2_wireframe50 max 0).
  Anything else draws through **WIRE twins**:
  - 8 main-pass soup programs, each with its own DEFORM twin; prewarmed at model swap.
  - Separate draws per mode / back (CLAUDE.md W5 table): Wire Only Hidden = a depth pre-pass with the same program, then the lines
    with LEQUAL.
- **Feature Edges** are flags built per model on a worker:
  - Vertices welded by position; an edge draws if it is open, non-manifold, or bends more than 20 degrees.
  - The fractured copy is handled per piece, so text and logo pieces keep their own edges.
  - Stored as an R8UI texture buffer on unit 25.
- **Glow / Bloom:**
  - The lines are drawn again into an HDR buffer with its own depth, and blurred at half size for the halo.
  - Both are added over the composite as light at alpha 0, before the post Bloom, so the global Bloom catches the wires as well.

## Combines with / does not
- **Combines:** Deform, Shatter / text pieces (each piece keeps its edges), Letters, clones (simplified LOD clones draw every edge),
  ghosts, the morph chain mid-hop (both models wired).
- **Not wired:** the Mirror Floor, the shadows and the other passes draw the SURFACE. Wire Only skips the projector picture and the
  lava cracks: there is no surface for them to land on. In Over Surface and Dark Fill they stay.

## Validated (evidence under verify/w5/, local)
- **Renders:** 48 shots = skull, SciFi helmet, text "LOADEHR", our SVG emblem x mode x back x edges, over the arena background
  (verify/w5/renders, renders.md).
- **Wire Only fills nothing.** Coverage = share of pixels with alpha > 0:

  | model | solid | Wire Only Mix 0 | Wire Only Mix 1 | Feature Edges |
  |---|---|---|---|---|
  | skull | 0.2182 | 0 | 0.2114 | 0.0357 |
  | helmet | 0.1914 | 0 | 0.1660 | 0.0662 |
  | text | 0.0344 | 0 | 0.0309 | 0.0133 |
  | logo | 0.2314 | 0 | 0.1848 | 0.0414 |

- **Feature Edges on a cube** (our generated test_models/gen/cube.obj): 24 of 36 triangle edges drawn, as predicted (12 cube edges x 2;
  the 6 diagonals left out). Line coverage 0.0411 -> 0.0201 (x0.49).
- **Wire Glow 0 / 3 / 10 with Post Bloom 2** (pale cyan lines): white-hot pixels (every channel >= 250) 3,447 -> 46,577 -> 48,804;
  frame brightness 78.5 -> 90.1 -> 92.0. The light goes past the colour and the global bloom catches it.
- **Wire Bloom 3 vs 0:** max difference 0 outside the halo's 25 px reach (150,725 px); 65,779 px changed inside it.
- **Cost, harness 1080p** (plugin ms / GPU ms avg, skull):

  | case | plugin ms | GPU ms |
  |---|---|---|
  | off | 1.97 | 0.84 |
  | W1 wire (plain) | 2.48 | 1.39 |
  | Over Surface + colour (twin) | 2.53 | 1.46 |
  | Wire Only Hidden | 3.16 | 1.71 |
  | Wire Only X-Ray | 2.66 | 1.54 |
  | Wire Only Feature | 2.75 | 1.69 |
  | Dark Fill Hidden | 2.42 | 1.39 |
  | X-Ray over a surface / fill | ~3.7 | ~2.6 |
  | + Glow 3 | 3.42 | 2.09 |
  | + Glow 3 + Bloom 3 | 3.71 | 2.40 |

  The helmet is similar (cost.md).
- **Arena 7.28, MBP M3 Max, Wire closed, one selected clip:** ~85 fps delivered with the wire off and on (the wire is not the limit).
  Plugin CPU: 1.12 ms off -> 1.26-1.34 ms Wire Only + glow (C1) -> 1.92 ms glow 10 + bloom 10 (C7).
- Regress max 0 (6 references); guards G1-G7 ok on all bundles. W3 DXV proof unchanged (13 files, max 1-4). W4 scrub sweep and cue
  pass (DXV, H.264).

## Arena clips (layer 9 "LoadEhr W5", opacity 1, LoadEhr Wow, soft spin, untriggered; selection parked on L9 C8)
Layer 8 "DXVKit W5 Forms" belongs to another project and was not touched.
- C1 W5 Wire Only Skull: Hidden, Feature Edges, cyan, Wire Glow 3, Bloom 1.5.
- C2 W5 Wire Only XRay: the same, X-Ray.
- C3 W5 Wire Text Bloom: text "LOADEHR", Wire Only, cyan, Wire Bloom 3 (Size 0.5), Glow 1.
- C4 W5 Dark Fill Helmet: SciFi helmet, Wire + Dark Fill, Feature Edges, amber, Glow 1.5.
- C5 W5 Over Surface Glow: skull, Over Surface, Mix 0.6, Glow 2 (the comparison to the W1 look).
- C6 W5 Wire Pieces: text, Wire Only, Feature Edges, Noise Warp 0.5, Shatter Mode Pieces, Shatter On Beat Every 4.
- C7 W5 Too Much: skull, all edges, Glow 10, Bloom 10, Size 1.

Dials: tools/arena_w5.py 9.

## Deviations / open, for Amrit's eyes
- Wireframe Mix stays the strength, so Wire Only needs Mix > 0 to show anything. The clips use 1. Should Wire Only default to full
  strength instead?
- The scanned skull has few hard creases, so its Feature Edges are sparse. Hard-surface models (the helmet, text, logos) read as
  panels.
- The Mirror Floor reflects the surface, not the wires.
- Silhouette moved to the classic bundles. A Wow clip that used Silhouette loses it; nothing else changed for Wow clips.
- W4 open questions still stand (where ~/Movies/KLV Set went).

## RECEIPTS
```
bff6cc7372d392c74c2630186992714393738e03 LoadEhr W5: wireframe only, wire colour, wire glow + bloom, in every bundle
## main...origin/main

ref-hash guard ok: 6 references match tools/regress_refs.sha256
compare verify/w1/ref/r1_mask_front.png vs …/r1_mask_front.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r2_wireframe50.png vs …/r2_wireframe50.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r3_explode.png vs …/r3_explode.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r4_fx_skin_env_input.png vs …/r4_fx_skin_env_input.png: mean 0.0000 max 0 pixels>2 0 of 
compare verify/w1/ref/r5_chain_mid_hop_b_c.png vs …/r5_chain_mid_hop_b_c.png: mean 0.0000 max 0 pixels>2 0 of 
compare verify/w1/ref/r6_chain_rest_d.png vs …/r6_chain_rest_d.png: mean 0.0000 max 0 pixels>2 0 of 2073600
G1 ok   LoadEhr: 501 params, none named Library* / Laser*
G6 ok   LoadEhr: no Mirror Room / Mirror Count / Box Size / LED* param
G4 ok   LoadEhr: covered pixels 0 ; 4 empty slots; get Status A = 'Nothing loaded: transparent output. Point M
G1 ok   LoadEhrFX: 505 params, none named Library* / Laser*
G6 ok   LoadEhrFX: no Mirror Room / Mirror Count / Box Size / LED* param
G4 ok   LoadEhrFX: covered pixels 0 ; 4 empty slots; get Status A = 'Nothing loaded: transparent output. Point
G1 ok   LoadEhrWow: 495 params, none named Library* / Laser*
G6 ok   LoadEhrWow: no Mirror Room / Mirror Count / Box Size / LED* param
G4 ok   LoadEhrWow: covered pixels 0 ; 4 empty slots; get Status A = 'Nothing loaded: transparent output. Poin
G1 ok   LoadEhrWowFX: 509 params, none named Library* / Laser*
G6 ok   LoadEhrWowFX: no Mirror Room / Mirror Count / Box Size / LED* param
G4 ok   LoadEhrWowFX: covered pixels 0 ; 4 empty slots; get Status A = 'Nothing loaded: transparent output. Po
G2 ok   the only directory API: ModelLoader.cpp FolderIndex (textures next to the chosen model file):
G3 ok   no model / texture file inside build/LoadEhr*.bundle (8 files: binaries + Info.plist)
G5 ok   no Laser* / Library* sources, no library tools
G6 ok   no Mirror* source, no MIRROR program variant
G7 ok   no libtess2 / tesselator / SGI-B in src, third_party, CMakeLists.txt
G7 ok   no LGPL file in the tree (619 tracked files, submodules included)

drwxr-xr-x  35 <user>  staff    1120 Oct  8 18:35 checks
drwxr-xr-x  13 <user>  staff     416 Oct  8 18:37 combos
drwxr-xr-x   5 <user>  staff     160 Oct  8 18:38 cost
drwxr-xr-x  55 <user>  staff    1760 Oct  8 18:37 renders
drwxr-xr-x  16 <user>  staff     512 Oct  8 18:41 w3proof
drwxr-xr-x   7 <user>  staff     224 Oct  8 18:40 w4_cue
drwxr-xr-x   7 <user>  staff     224 Oct  8 18:40 w4_sweep
      48

LoadEhr W5 opacity 1.0
 C1 W5 Wire Only Skull (Disconnected)
 C2 W5 Wire Only XRay (Disconnected)
 C3 W5 Wire Text Bloom (Disconnected)
 C4 W5 Dark Fill Helmet (Disconnected)
 C5 W5 Over Surface Glow (Disconnected)
 C6 W5 Wire Pieces (Disconnected)
 C7 W5 Too Much (Disconnected)
Arena PID 43403

$ ls -la verify/w5 (dirs)
drwxr-xr-x  35 <user>  staff    1120 Oct  8 18:35 checks
drwxr-xr-x  13 <user>  staff     416 Oct  8 18:37 combos
drwxr-xr-x   5 <user>  staff     160 Oct  8 18:38 cost
drwxr-xr-x  55 <user>  staff    1760 Oct  8 18:37 renders
drwxr-xr-x  16 <user>  staff     512 Oct  8 18:41 w3proof
drwxr-xr-x   7 <user>  staff     224 Oct  8 18:40 w4_cue
drwxr-xr-x   7 <user>  staff     224 Oct  8 18:40 w4_sweep
```
END OF HANDOFF w5-wire sha256:db9b1f64 9767
