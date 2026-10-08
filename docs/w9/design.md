# LoadEhr W9: the looks pack: design notes (written before each stage's code)

## Param room: the four bundles (decided before Stage A)
Before W9: LoadEhr 504, FX 508, Wow 498, Wow FX 512. Shared = 330 params, classic-only 174, Wow-only 168; FX extras: Input 3 (both FX),
Project Input 1 (LoadEhr FX), Face 10 + Proj Source 1 (Wow FX). Both FX carriers together: 2 x shared + both sides' extras <= 1024,
and it was 1020. W9 needs 20 controls (Lines 5, Build 5, Cut 4, Surface Look 5, Shape Target 1).
- Every new control shared would need 40 more in that sum: 36 shared params would have to become one-sided. Making the new groups
  one-sided needs at least 17 (2 x 1 + 19 - 4).
- The move that frees the most and matches the families: **Smoke Sim (30) -> Wow only** (the sim is what Trip and Fire are built on;
  the classic bundles keep the Noise smoke, the Volume group, every light). Project Input (projects the FX input onto the SIM smoke)
  goes with it: LoadEhr FX -> Wow FX. In the classic bundles the Smoke transition style dissolves (as Swarm / Burn / Lava already do).
- **Face (10) Wow FX -> LoadEhr FX** (it was moved to Wow FX in W1 for room only; tracking masks onto faces in the input is object
  handling).
- New: **Lines, Build, Surface Look -> classic** (LoadEhr / FX: the object-handling side, with Shape / Copies / Letters).
  **Cut (4), Build Axis (1, the cut's axis too), Shape Target (1, Deform) -> every bundle.** In Wow the Build group shows only Build Axis.
- After W9: LoadEhr 494, FX 507, Wow 504, Wow FX 509 (every stage's count in the results). No fifth bundle.
- Saved clips: a moved group keeps its saved VALUES in the clip (Arena stores them by name), but a bundle that no longer carries it
  runs it at its default (off): a classic clip with Smoke Engine Sim loses the sim smoke; a Wow FX clip with Face Track loses it.

## Architecture (all stages)
- VERTEX + FRAGMENT looks live in the DEFORM twins (W2: every soup program, plain / instanced / letters / morph / shatter transition /
  bounce / cracks / wire / points / the shadow passes, already has one, used only while a deform is on). A W9 look switches the twin on
  (shape.deform). So every W9 shape look follows clones, letters, the morph pair, the wire / points looks and the shadows without a single
  new program family. The plain programs' source is unchanged (regress max 0). The FS half: the DEFORM twins get `#define DEFORM` in the
  FS too, and the W9 shading sits under it.
- CAPTURE twins (new, Stage A): the POINTS soup stage with `#define CAPTURE` and transform feedback (rasterizer discard): any set of
  surface spots (triangle, b1, b2) -> this frame's world position, normal and base colour, through the SAME soup stage as the model
  (deforms, letters, shard flight, morph). Lines, ribbons, tubes, spikes and skin copies all read these buffers.
- New passes draw into the main MSAA target (depth tested against the model) as light (rgb added, alpha kept: W23), and again into the
  W5 HDR wire buffer for glow / bloom.

## Stage A: Lines (group "Lines", classic): Line Look Off / Plexus / Light Paint / Neon Tubes, Line Amount, Line Reach, Line Speed,
## Line Audio; colour / thickness / glow / bloom from the Wireframe group
Reading: Plexus (Rowbyte, After Effects): points joined by lines when closer than a max distance, line opacity a curve of the distance
(cables.gl ArrayPointsProximity: alpha = 1 - d / dMax). Light painting: a light source's path exposed over time (long exposure); here
real 3D trails of surface points kept as positions over time. Neon: a hot white core and a coloured falloff across the tube; the
tube's light on nearby surfaces.
- PLEXUS. Points = a subset of the Point Source set (Line Amount: 200 + Amount^2 x 20000 points). Neighbours: OFF the render thread on the
  CPU, at rest in fitted space (a uniform grid of cell = the reach, the 8 nearest within the reach per point, pairs i < j), rebuilt when
  the reach or the points change. Each frame the points are captured through the soup stage (they follow deforms / spin / shatter /
  letters / morph), and every pair draws as a screen-space quad (Wire Thickness px) whose brightness falls with the CURRENT distance
  ((1 - d / reach)^2): shards flying apart stretch their lines and fade them out. Line Audio: the FFT (16 bands, bass at the model's
  base, highs at its top) pulses the reach and the brightness. Line Speed: light pulses running along the lines. Nodes: dots.
- LIGHT PAINT. N heads (20 + Amount^2 x 3000 surface points). Their captured world positions go into a ring (60 samples / s of show time,
  up to 4 s: Line Reach x 4 s), each sample stamped with its time; a ribbon per head through its past positions, camera-facing, fading
  with age. World space: the camera can orbit round the trails. Line Speed: each head circles its surface spot (a tangent-plane swirl,
  whole turns per 1000 s per head: W14) so a still model paints too.
- NEON TUBES. The W5 feature edges (creases > 20 deg, open edges) as edge-end pairs, captured, drawn as camera-facing tubes (white-hot
  core, coloured glow). They light the model through the glow buffer: the tubes' light blurred wide (Line Reach) and added onto the
  model's own picture (x its colour), light at alpha 0. A screen-space approximation (say so): no shadowing, light only where the model
  is on screen. Line Speed: pulses flowing along the tubes + flicker; Line Audio: the FFT pulses the tubes.
- Mid-hop: both models capture their own points (through their MORPH programs); the outgoing model's lines fade out and the incoming
  one's fade in across the hop's middle (t 0.35 .. 0.65).
- Text in Shatter Mode Pieces: the lines stay on the whole letters (the points sit on the whole mesh; the pieces are a separate copy).
