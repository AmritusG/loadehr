# 3D smoke recipe book (W22, re-tuned W23; MaskEhr's, LoadEhr W1 port)

LoadEhr W1: in LoadEhr a recipe's model is a FILE a Model slot points at, by path (`tools/w22_smoke.py`: files in MaskEhr's
model folder, on this Mac only; MaskEhr's recipes on its carnival mask use Masks/Plague Mask here: that mask is forbidden). There is no library. Layers / columns below are
MaskEhr's Arena clips. `tools/parity.py` renders every recipe here in MaskEhr and LoadEhr.

Twelve ready looks for the Smoke Sim engine (W20). Each one is a clip in Arena, layer **"Smoke Examples"**, columns 1-12,
in this order. Each was tuned by eye in the harness (5 s at 1080p, frames at 1, 2, 3 and 5 s). The W23 contact sheet over a
textured background is `verify/w23/after/contact.png`. The W22 sheets are over black (`verify/w22/contact.png`).
The source of truth is `EXAMPLES` in `tools/w22_smoke.py`. (MaskEhr's `tools/w22_arena.py` dialled them into its layer.)

## W23: smoke in the air (read this first)
- **Arena showed the smoke only ON the mask** until W23. Both bundles declared FFGL API 2.1, and below 2.3 Arena reads a SOURCE
  plugin's output as STRAIGHT alpha: it multiplies rgb by alpha. Smoke, beams, glow cloud, motes, bloom and flares in the air
  add light at alpha 0, so they were multiplied away. Only the parts over the opaque mask survived. W23 declares API 2.3:
  premultiplied in and out. The proof is `verify/w23/composite_before_*.png`, with the straight reading next to the premultiplied one.
- The W22 tuning was judged over black, where both readings look the same. The harness now flattens `shot` frames over a textured
  background by default (`composite arena`). `composite straight` shows the old Arena reading. `composite black` is the W1
  regression look.
- The sim was never the problem: 61-98 % of each recipe's smoke was already more than 0.15 mask radii off the surface after 5 s
  (`tools/w23_air.py air`; the table is below).
- **Smoke Ambient** (Volume) makes smoke scatter the key light's colour from everywhere, so smoke in the air reads without a beam.
  **Smoke Key Light** (Volume) lets the key light light the smoke, with the mask's key shadow. Either one, with Smoke Amount > 0,
  runs the volume even with no volume light on. Both work with Vol Method Raymarch only.
- **Emit Offset** (Smoke Sim, in mask radii) starts Rim and Eyes smoke this far out of the surface. On Smoulder, the air fraction
  goes 0.74 -> 0.79 at 0.15 and 0.93 at 0.30.
- To see smoke over content in Arena: trigger any generator clip on a layer BELOW the smoke layer (clip "Air Check" is the Fog
  Bank recipe for exactly that).

## What the twelve taught
- **Without Smoke Ambient / Smoke Key Light, smoke shows only where a volume light reaches it.** Every recipe has
  Light A, B or C on. A **point** light (A/B Type Point) lights the whole plume. A spot only lights its cone.
- **Haze 0, Glow Cloud 0** in every recipe. Otherwise the haze and the glow ball hide the plume (the W20 finding holds).
- **Smoke Amount** is the visibility dial (density in the render, not in the sim). Use 2-4 for plumes and 1-1.5 for a lit interior.
- **Smoke Fade** sets the plume's shape. Around 0.2 makes a long trail that fills the frame (Incense). Around 1-1.2 keeps a plume near its
  source (Dragon, Skull, Letters). 2 or more makes it thin and short-lived.
- **Point the camera across the flow.** Eye smoke leaves along the mask's front, so from straight ahead it comes at you
  and barely reads. Orbit Yaw 20-40 turns it into a side-on plume (Dragon, Exhale, Skull).
- **Back light the medium** for a silhouette or aura. Put the light behind the mask (A Pos Z -1.2 to -2.5). Forward scattering
  (Scatter G 0.6) makes the smoke between the light and the camera glow (Fog Bank, Smoulder, Letters, Audio Fog).
- **Sim + Detail** adds wisps for slow, close looks. **Sim** is cleaner for bursts and lit interiors.
- **Hollow models**: Emit Inside fills the SDF cavity, and the smoke leaves through any opening wider than about one sim cell. The
  Ding Censer's pierced lid makes beams through the holes at 64^3. The Birdcage's bars need **Sim Res 96** before the smoke
  gets out (at 64^3 it stays trapped inside: smoke total 5.4 cell^3 sitting at the cage centre).
- **No eye pair found?** Place Light C by hand: C Mirror X Off, then set Left / Right Pos (Ghost Whisper on the Plague Mask).
  Emit Eyes uses the hand-placed C positions.

## Recipes (non-default dials; colours are one HSBA picker each, H/S/B/A 0..1)
| # | Example | Model | Dials (non-default) |
|---|---|---|---|
| 1 | Dragon Breath | Masks/Plague Mask | Smoke Engine Sim + Detail, Haze 0, Glow Cloud 0, Smoke Amount 4, Emit Eyes 10, Eyes Heat 2, Smoke Fade 0.8, Vorticity 3, Sim Box 2, Orbit Yaw 40, Orbit Pitch 5, Distance 5, Smoke Color H 0.07 S 0.8 B 1 A 1, A Enable On, A Type Point, A Anchor To Mask Off, A Pos X -0.5, A Pos Y 1.2, A Pos Z 2, A Intensity 40, A Color H 0.07 S 0.7 B 1 A 1, C Enable On, C Intensity 4, C Left Color H 0.07 S 0.85 B 1 A 1, Emit Offset 0.15, Smoke Ambient 0.3 |
| 2 | Incense | Lanterns/Ding Censer | Smoke Engine Sim, Haze 0, Glow Cloud 0, Smoke Amount 3, Emit Inside 6, Inside Heat 3, Smoke Fade 0.2, Buoyancy 2.5, A Enable On, A Type Point, A Pos Z 0, A Intensity 12, A Color H 0.08 S 0.6 B 1 A 1, B Enable On, B Type Point, B Anchor To Mask On, B Pos X 0.5, B Pos Y 1.5, B Pos Z 1.5, B Intensity 10, B Color H 0.08 S 0.4 B 1 A 1, Smoke Ambient 0.5 |
| 3 | Fog Bank | Masks/Plague Mask | Smoke Engine Sim + Detail, Haze 0, Glow Cloud 0, Emit Behind 8, Buoyancy 0.3, Wind X 0.8, Sim Box 3.5, A Enable On, A Pos Z -2.5, A Intensity 20, A Cone Angle 70, A Color H 0.58 S 0.6 B 1 A 1, Key Intensity 0.3, Env Intensity 0.15, Smoke Ambient 0.8 |
| 4 | Smoulder | Masks/Plague Mask | Smoke Engine Sim + Detail, Haze 0, Glow Cloud 0, Smoke Amount 2, Emit Rim 5, Rim Heat 4, Smoke Fade 1, Smoke Color H 0.06 S 0.5 B 1 A 1, Key Intensity 0.8, Rim Glow 2, Rim Color H 0.06 S 0.8 B 1 A 1, A Enable On, A Type Point, A Anchor To Mask Off, A Pos Y 1, A Pos Z -1.2, A Intensity 4, A Color H 0.06 S 0.7 B 1 A 1, Emit Offset 0.25, Smoke Ambient 0.6 |
| 5 | Ghost Whisper | Masks/Plague Mask | Smoke Engine Sim + Detail, Haze 0, Glow Cloud 0, Smoke Amount 4, Emit Eyes 4, Eyes Heat 0.6, Vorticity 7, Buoyancy 1, Smoke Color H 0.33 S 0.35 B 1 A 1, C Mirror X Off, C Left Pos X -0.13, C Left Pos Y 0.21, C Left Pos Z 0.55, C Right Pos X 0.27, C Right Pos Y 0.21, C Right Pos Z 0.55, C Enable On, C Intensity 3, C Left Color H 0.33 S 0.45 B 1 A 1, A Enable On, A Type Point, A Anchor To Mask Off, A Pos Y 1.5, A Pos Z 2, A Intensity 18, A Color H 0.33 S 0.3 B 1 A 1, Emit Offset 0.1, Smoke Ambient 1.5 |
| 6 | Exhale On Beat | Masks/Plague Mask | Smoke Engine Sim, Haze 0, Glow Cloud 0, Smoke Amount 4, Emit Eyes 6, Emit Behind 1, Eyes Heat 1, Beat Puff Every 2, Puff Amount 10, Smoke Fade 1.2, C Enable On, C Intensity 2, Orbit Yaw 40, Orbit Pitch 5, Distance 5, A Enable On, A Type Point, A Anchor To Mask Off, A Pos X 0.8, A Pos Y 2, A Pos Z 2.5, A Intensity 40, Smoke Ambient 0.8 |
| 7 | Vortex | Masks/Plague Mask | Smoke Engine Sim + Detail, Haze 0, Glow Cloud 0, Smoke Amount 3, Spin Y 12, Emit Behind 8, Smoke Fade 1, Vorticity 6, Buoyancy 0.8, Orbit Pitch 25, A Enable On, A Type Point, A Anchor To Mask Off, A Pos X 0.8, A Pos Y 2, A Pos Z 2.5, A Intensity 25, Smoke Ambient 0.4, Smoke Key Light 1 |
| 8 | Caged Smoke | Lanterns/Birdcage | Smoke Engine Sim, Haze 0, Glow Cloud 0, Sim Res 96, Smoke Amount 1.5, Emit Inside 3, Inside Heat 4, Smoke Fade 0.3, Wind X 0.3, Orbit Pitch 10, A Enable On, A Type Point, A Pos Z 0, A Intensity 6, B Enable On, B Type Point, B Pos X 1, B Pos Y 2.5, B Pos Z 1.5, B Intensity 20, B Color H 0.1 S 0.3 B 1 A 1, Smoke Ambient 0.4 |
| 9 | Skull Breath | Skulls/Human Skull | Smoke Engine Sim + Detail, Haze 0, Glow Cloud 0, Emit Eyes 5, Eyes Heat 2.5, Smoke Fade 1, Orbit Yaw 20, Smoke Color H 0 S 0.6 B 1 A 1, C Enable On, C Intensity 5, C Left Color H 0 S 0.95 B 1 A 1, Emit Offset 0.15, Smoke Ambient 1.5, Smoke Amount 3 |
| 10 | Smoking Letters | Text | Model Source Text, Text MASKEHR, Metal Override On, Brightness 0.35, Roughness Scale 0.6, Smoke Engine Sim + Detail, Haze 0, Glow Cloud 0, Emit Rim 6, Rim Heat 4, Buoyancy 3, Smoke Fade 1.2, Smoke Amount 2, A Enable On, A Pos Z -2, A Intensity 10, A Color H 0.58 S 0.3 B 1 A 1, Emit Offset 0.2, Smoke Key Light 1.5 |
| 11 | Projected Smoke | Masks/Plague Mask (MaskEhr FX on a generator) | Smoke Engine Sim, Haze 0, Glow Cloud 0, Smoke Amount 3, Emit Behind 6, Emit Rim 3, Project Input 1, A Enable On, A Type Point, A Anchor To Mask Off, A Pos X 0.8, A Pos Y 2, A Pos Z 2.5, A Intensity 25, Smoke Ambient 0.5 |
| 12 | Audio Fog | Masks/Plague Mask | Smoke Engine Sim + Detail, Haze 0, Glow Cloud 0, Emit Behind 1, Audio > Emit 2, Buoyancy 0.5, Smoke Fade 1, A Enable On, A Pos Z -2.5, A Intensity 10, A Cone Angle 70, A Color H 0.6 S 0.35 B 1 A 1, Smoke Ambient 0.8 |

## Smoke in the air (W23, after 5 s; air = share of the smoke more than 0.15 mask radii off the surface; height = p90 above the mask centre, in radii)
| # | Example | air before | air after | height before | height after |
|---|---|---|---|---|---|
| 1 | Dragon Breath | 0.843 | 0.906 | 1.28 | 0.91 |
| 2 | Incense | 0.974 | 0.974 | 2.15 | 2.15 |
| 3 | Fog Bank | 0.979 | 0.979 | 1.59 | 1.59 |
| 4 | Smoulder | 0.743 | 0.886 | 2.15 | 2.07 |
| 5 | Ghost Whisper | 0.961 | 0.973 | 1.37 | 1.37 |
| 6 | Exhale On Beat | 0.891 | 0.891 | 1.91 | 1.91 |
| 7 | Vortex | 0.911 | 0.911 | 1.29 | 1.29 |
| 8 | Caged Smoke | 0.610 | 0.610 | 1.59 | 1.59 |
| 9 | Skull Breath | 0.885 | 0.930 | 1.99 | 1.52 |
| 10 | Smoking Letters | 0.857 | 0.897 | 2.07 | 2.07 |
| 11 | Projected Smoke | 0.927 | 0.927 | 2.07 | 2.07 |
| 12 | Audio Fog | 0.934 | 0.938 | 0.98 | 0.90 |

Caged Smoke stays at 0.61 by design: the smoke is meant to be IN the cage, and most points inside it are within 0.15 radii of a bar. More buoyancy or wind lowered it (0.53-0.59). The beat / audio recipes (Exhale, Audio Fog) vary by about 0.005 between runs.

Notes
- 96^3 is used once: **Caged Smoke**. Every other recipe runs at 64^3, the default.
- **Incense** uses **Lanterns / Ding Censer**, which is open under a pierced lid. The kickoff's first pick, Incense Burner, kept its smoke inside and below the
  bowl (smoke total 0.19 cell^3, centroid y -0.9), so no plume showed.
- **Projected Smoke**: MaskEhr FX as an effect on Resolume's **Abstract Field** generator. In the harness, a soft pink / cyan /
  yellow / violet gradient (`verify/w22/input_gen.png`) stands in for the generator.
- **Exhale On Beat**: with Puff Amount 10, each puff raises the total smoke about 5x (measured 640 -> 1600-3700 cell units).
  On screen this reads as the cloud swelling every 2 beats, not as separate puffs (see open ideas in the W22 handoff).
- **Audio Fog**: the harness feeds an FFT envelope that decays every beat (`fft 1`). In Arena, Audio FFT = Composition.


## W24 Trip looks (Arena L25 "Trip Smoke", columns 1-14, untriggered)
Each Trip look = a recipe above + the Trip dials (later dials win). Trip Master (default 1) scales every look: ride it as the
craziness fader (0 = the plain recipe, 2 = too much on purpose). Time Warp / Freeze are a speed, not scaled by it.
Medium looks (colour, film, kaleido, fractal, veins, contours, blacklight) are Raymarch only; Prism and Hue Echo work with both.
Renders: `verify/w24/trip/<nn>_<slug>/strip.png` (1, 2, 3, 5 s), `master.png` (Trip Master 0 / 0.5 / 1), `verify/w24/trip/contact.png`.

| # | Look | Base recipe | Trip dials (on top of the base) |
|---|---|---|---|
| T1 | Rainbow Swirl | 1 Dragon Breath | Smoke Color H 0 S 0 B 1 A 1, Smoke Amount 2, A Intensity 15, A Color H 0.07 S 0.1 B 1 A 1, C Left Color H 0.07 S 0.15 B 1 A 1, Colour By Swirl, Trip Palette Rainbow, Palette Shift 0.1, Palette Spread 1.5 |
| T2 | Oil Slick | 3 Fog Bank | Smoke Color H 0 S 0 B 1 A 1, Smoke Ambient 0.25, A Intensity 8, A Color H 0.58 S 0.1 B 1 A 1, A Intensity 3, Smoke Ambient 0.6, Thin Film 1.2, Thin Film Thick 1.2 |
| T3 | Prism | 1 Dragon Breath | Smoke Color H 0 S 0 B 1 A 1, Smoke Amount 2, A Intensity 15, A Color H 0.07 S 0.1 B 1 A 1, Prism 1.5, Prism Angle 0 |
| T4 | Mandala | 3 Fog Bank | Smoke Color H 0 S 0 B 1 A 1, Smoke Ambient 0.25, A Intensity 8, A Color H 0.58 S 0.1 B 1 A 1, Kaleido Smoke 8, Kaleido Spin 4, Colour By Height, Trip Palette Neon, Palette Shift 0.05 |
| T5 | Fractal Fog | 3 Fog Bank | Smoke Color H 0 S 0 B 1 A 1, Smoke Ambient 0.25, A Intensity 8, Fractal Fog 1, Fractal Scale 1, Fractal Iter 3, Fractal Morph 6 |
| T6 | Neon Veins | 4 Smoulder | Neon Veins 4, Vein Width 0.5, Vein Color H 0.5 S 1 B 1 A 1 |
| T7 | Contours | 3 Fog Bank | Smoke Color H 0 S 0 B 1 A 1, Smoke Ambient 0.25, A Intensity 8, A Color H 0.58 S 0.1 B 1 A 1, A Intensity 2, Smoke Ambient 0, Smoke Contours 4, Contour Bands 5, Band Sharpness 0.9, Band Scroll 0.3, Trip Palette Rainbow |
| T8 | Blacklight | 1 Dragon Breath | Smoke Color H 0 S 0 B 1 A 1, A Intensity 6, C Intensity 1, Smoke Ambient 0, Blacklight 3, Blacklight By Heat, Blacklight Color H 0.76 S 0.85 B 1 A 1 |
| T9 | Smoke Rings | 1 Dragon Breath | Smoke Color H 0 S 0 B 1 A 1, Smoke Amount 2, A Intensity 15, A Color H 0.07 S 0.1 B 1 A 1, Orbit Yaw 70, Sim Res 96, Smoke Fade 1.5, Emit Eyes 0, C Intensity 1, Ring On Beat Every 2, Ring From Eyes, Ring Size 0.6, Ring Speed 3, Ring Smoke 4 |
| T10 | Ghost Faces | 1 Dragon Breath | Smoke Color H 0 S 0 B 1 A 1, Smoke Amount 2, A Intensity 15, A Color H 0.07 S 0.1 B 1 A 1, Orbit Yaw 0, Sim Res 96, Emit Eyes 0.5, Smoke Ghosts 0.6, Ghost Offset 1, Ghost Scale 1.3, Ghost Fade Time 1.5 |
| T11 | Hyperspace | 3 Fog Bank | Smoke Color H 0 S 0 B 1 A 1, Smoke Ambient 0.25, A Intensity 8, A Color H 0.58 S 0.1 B 1 A 1, Hyperspace 2, Colour By Speed, Trip Palette Rainbow, Palette Shift 0.1 |
| T12 | Hue Echo | 6 Exhale On Beat | Hue Echo 0.9, Echo Hue Step 0.02 |
| T13 | Time Warp | 1 Dragon Breath | Time Warp 0.4 |
| T14 | Bonanza | 1 Dragon Breath | Smoke Color H 0 S 0 B 1 A 1, Smoke Amount 2, A Intensity 15, A Color H 0.07 S 0.1 B 1 A 1, C Left Color H 0.07 S 0.15 B 1 A 1, Trip Master 0.7, Colour By Swirl, Trip Palette Rainbow, Palette Shift 0.15, Palette Spread 1.5, Kaleido Smoke 6, Kaleido Spin 3, Thin Film 0.5, Neon Veins 2, Hue Echo 0.85, Prism 0.5, Whirlpool 1, Ring On Beat Every 4 |

### T14 Bonanza, the whole recipe (the all-in DMT look, Trip Master 0.7)
Model: Masks/Plague Mask. Smoke Engine Sim + Detail, Haze 0, Glow Cloud 0, Emit Eyes 10, Eyes Heat 2, Smoke Fade 0.8, Vorticity 3, Sim Box 2, Orbit Yaw 40, Orbit Pitch 5, Distance 5, A Enable On, A Type Point, A Anchor To Mask Off, A Pos X -0.5, A Pos Y 1.2, A Pos Z 2, C Enable On, C Intensity 4, Emit Offset 0.15, Smoke Ambient 0.3, Smoke Color H 0 S 0 B 1 A 1, Smoke Amount 2, A Intensity 15, A Color H 0.07 S 0.1 B 1 A 1, C Left Color H 0.07 S 0.15 B 1 A 1, Trip Master 0.7, Colour By Swirl, Trip Palette Rainbow, Palette Shift 0.15, Palette Spread 1.5, Kaleido Smoke 6, Kaleido Spin 3, Thin Film 0.5, Neon Veins 2, Hue Echo 0.85, Prism 0.5, Whirlpool 1, Ring On Beat Every 4

Notes
- The base recipes are tuned bright; the coloured looks lower Smoke Ambient / lamp intensity so the smoke's core is not
  tonemapped to white (white takes no colour).
- Smoke Rings read best side-on (Orbit Yaw 70) and at Ring Size >= 0.5: a smaller torus is only a few cells of the grid.
- Freeze is a held button: the smoke stands still in the world while the mask keeps moving.
