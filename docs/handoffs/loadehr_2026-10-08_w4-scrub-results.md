project:    LoadEhr
pass:       w4-scrub
STATUS:     COMPLETE
parent:     7b47a41
result:     e65b2c7
session:    3b26eb0d-b573-42d5-b913-073ed46d4d76
written_at: 2026-10-08T18:25:00+02:00
# LoadEhr W4 results: a time scrubber for the projector's video

## Amrit's answers this session
- W3 verdict (from the brief, verbatim): "8 - 9 identical as far as I can tell. 10 - 11 looks proper. 1 - 7 plays properly." So
  DXVKit's picture (C8) matches Arena's own DXV decode (C9) by eye, Video Alpha behaves, and all 7 DXV variants play.
- Restarted Arena three times when asked (PIDs 77631 -> 29914 -> 71748 (force quit, see below) -> 25852).
- Hand test of the moving playhead (L7 C5): "All good". It moves smoothly, his drag wins, it plays on from the drop, nothing odd.

## What shipped (commit e65b2c7, pushed; Commons 99fcef4)
- Video Loop gains **Scrub** (Loop / Ping-Pong / Once / Scrub). In Scrub there is no clock: the frame shown = round(Video Position x
  (N - 1)). Until that frame is decoded the last one stays (never black). From the 3rd render waiting it shows the decoded frame nearest
  the target on the way, so a fast drag on a long-GOP file moves instead of freezing.
- **Video Position** (STD 0..1, Wow + Wow FX). In Loop / Ping-Pong / Once a user change CUES: the clock jumps to that frame, waits on
  it until it is on screen (max 1 s), then plays on. **PLAYHEAD:** while playing, the plugin pushes its own position at most 15 times a
  second. A host set equal to one of the last 8 pushes counts as an echo, not a cue. A restart pending in the same frame wins, so a
  clip load's saved position does not cue.
- **Video Time** (read-only TEXT, 5 / s): "mm:ss.f / mm:ss.f  frame n / N  codec".
- **Video Speed -4..4** (widened, W25-safe). Plays backwards in every mode; Once at a negative speed restarts from the end and holds
  frame 0.
- AVFoundation seeks:
  - An AVSampleCursor pass at open lists the key frames.
  - The worker reads on with the open reader vs opens a new one, whichever delivers fewer frames.
  - Backward chunks are kept whole (window = the near 6 frames + a chunk of up to 30 / ~256 MB).
  - Scrub backward reads at least 6 frames per reader.
- Counts: LoadEhr 482, FX 486, **Wow 490, Wow FX 504** (<= 512). Names <= 16 chars. CLAUDE.md W4 section + param map updated.
- Commons:
  - LOADEHR-007: Arena shows a plugin-pushed moving slider + live TEXT; a user drag wins; no echo.
  - LOADEHR-008: AVAssetReader stamps the first sample with the range start (off-by-one).
  - LOADEHR-009: seek API costs, and two dead ends.

## Deviations, and why
- **Bug found + fixed (since W2):** AVAssetReader hands out the frame covering the range start stamped WITH the range start. The W2
  open at (start - 0.5) / fps therefore showed every BACKWARD read one frame early (ProRes: wanted 145, got picture 144). It now opens
  at + 0.25 frame. Probe: pts 99.50 carries picture 99.
- **Dead end 1, AVPlayer + AVPlayerItemVideoOutput:** its seeks are 1.1-3x faster than a new reader, but it needs the main run loop.
  From a worker with no main run loop it never reached ReadyToPlay in 3 s.
- **Dead end 2, a random-access reader re-aimed with resetForReadingTimeRanges (~10x faster per jump; ProRes 1.8 ms):**
  - It CORRUPTED MediaToolbox's heap: 3 harness crashes (malloc freelist; "pointer being freed was not allocated" under
    resetForReadingTimeRanges).
  - In Arena it left a video worker deadlocked inside copyNextSampleBuffer; Arena stopped answering at 0 % CPU.
  - **Amrit had to force-quit Arena (~17:25).** Removed. Layer 7 survived the relaunch.
- Brief asked "Play Once & Hold" for C1's timeline: Arena's param timeline offers no such mode (Loop / Bounce / Random / Play Once &
  Clear). C1 uses Play Once & Clear, which holds the end value (W28). Re-click restarts it.
- **~/Movies/KLV Set vanished mid-session** (AWAKE & DISSOLVE, THE GREAT SURRENDER: not on disk, not in the Trash; I did not touch
  it). Consequences:
  - C1 / C5 were moved to black_snowflake (DXV DXT5 4K, 15.03 s).
  - The W3 proof re-ran on the 13 remaining files. **Amrit: was that folder moved on purpose?** W3 clips L5 C1 / C3 point into it.
- Also found and fixed during W4 (these never shipped): a 1-frame clip never decoded (my window rewrite); a cue showed one
  in-between frame.

## Validated (evidence under verify/w4/, local)
- **Scrub sweep 0 -> 1 -> 0, 75 steps each way, frame-coded clips (the picture carries its frame number):**
  - Every round(p x (N - 1)) shown, in order, picture code = decoder index, on DXV / HAP / HAP Q / ProRes / H.264 (one key frame) /
    H.264 (key every 30) / HEVC, at 1080p, and on all of them at 4K except one (below).
  - In-between pictures appear only on the way, never from the wrong side.
  - **Exception: H.264 4K with ONE key frame** cannot follow a 0.25 s backward step (each seek decodes from frame 0: ~110-390 ms).
- **Cue to 0.5 while playing:** 14 / 14 land on frame 75 and count on (75, 76, 77, ...).
- **Reverse (speed -1):** 42 / 42 run backwards frame by frame in Loop / Ping-Pong / Once. Once holds frame 0. The only skips are 1-10
  frames at the first turn on 4K long-GOP / HAP 4K.
- **Long show, Loop, 5 s clip, 180 min:** speed +1 -> frame 0 and speed -1 -> frame 149 at 60 / 120 / 180 min. Exact (W14).
- Regress max 0 (6 references); guards G1-G7 ok on all four bundles. W3 DXV proof: 13 files, max 1-4 of 255 (as W3).

### Scrub latency
Request -> frame on screen, harness 60 Hz, median / p95 ms, 24 random jumps per file:

| file | 1080p | 4K |
|---|---|---|
| DXV | 20.7 / 22.4 | 20.3 / 21.4 |
| HAP | 20.6 / 21.5 | 40.2 / 43.2 |
| HAP Q | 19.9 / 21.7 | 41.7 / 43.7 |
| ProRes | 61.8 / 81.7 | 86.5 / 107.8 |
| H.264, key every 30 | 44.1 / 79.9 | 89.6 / 143.4 |
| H.264, one key | 64.7 / 125.7 | 111.5 / 267.9 |
| HEVC, one key | 77.9 / 141.0 | 110.9 / 267.0 |

One render is ~17-20 ms. Drags (120 steps, one per render): the frames are prefetched, 0 ms, 0 renders held.

### Arena
7.28, MBP M3 Max, Wire closed, 120 Hz panel, single selected clip; plugin PERF fps (call rate) / CPU ms:
- Scrub drag over REST at ~50 / s: HAP 1080 88-125 fps / 0.4-1.1 ms (383 / 393 targets shown at once); H.264 1080 85-121 / 0.45-1.5
  (224 / 392).
- DXV under its BPM animation: 87-94 / 0.9-1.3 (338 targets, 0 ms).
- Pushes: REST read 75 distinct positions in 80 polls; Arena wrote back 0 of ~120 pushed values in 8 s.
- Cue PUT 0.5 on a 6650-frame clip -> frame 3325 in 8.9 ms, played on.

## Arena clips (layer 7 "LoadEhr W4", opacity 1, LoadEhr Wow, skull in haze, untriggered; selection parked on L7 C8)
- C1 W4 Scrub Transport: snowflake DXV, Scrub; Video Position on Arena's TIMELINE (Play Once & Clear, Restart, 15.0333 s, 0 -> 1).
- C2 W4 Scrub BPM Loop: YCG6-DXV3 (10 s), Scrub; Video Position BPM SYNC loop, 16 beats.
- C3 W4 Scrub HAP: hap_1080.mov, Scrub, position 0.5, ready to drag.
- C4 W4 Scrub H264: h264_1080.mp4 (one key frame), Scrub, position 0.5, ready to drag.
- C5 W4 Loop Playhead: snowflake DXV, Loop, moving playhead.
- C6 W4 Reverse PingPong: YCG6-DXV3, Ping-Pong, Video Speed -1.

Dials: tools/arena_w4.py 7. Earlier clips were not touched.

## Open / for Amrit's eyes
- Where did ~/Movies/KLV Set go? The W3 clips L5 C1 / C3 need it.
- Scrubbing H.264 / HEVC is slow by nature (decodes from the key frame); DXV / HAP scrub in one render. Prune Scrub to those codecs?
- Not checked: Arena undo of pushed values; a MIDI-mapped Video Position.

## RECEIPTS
```
e65b2c72634bcfdda2c9c5af01f992671602b36b LoadEhr W4: a scrubber for the projector's video (Video Position, Scrub, playhead, Video Time, reverse speed)
## main...origin/main

ref-hash guard ok: 6 references match tools/regress_refs.sha256
compare verify/w1/ref/r1_mask_front.png vs …/r1_mask_front.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r2_wireframe50.png vs …/r2_wireframe50.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r3_explode.png vs …/r3_explode.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r4_fx_skin_env_input.png vs …/r4_fx_skin_env_input.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r5_chain_mid_hop_b_c.png vs …/r5_chain_mid_hop_b_c.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r6_chain_rest_d.png vs …/r6_chain_rest_d.png: mean 0.0000 max 0 pixels>2 0 of 2073600
G1 ok   LoadEhr: 482 params, none named Library* / Laser*
G6 ok   LoadEhr: no Mirror Room / Mirror Count / Box Size / LED* param
G4 ok   LoadEhr: covered pixels 0 ; 4 empty slots; get Status A = 'Nothing loaded: transparent output. Point Model A (or
G1 ok   LoadEhrFX: 486 params, none named Library* / Laser*
G6 ok   LoadEhrFX: no Mirror Room / Mirror Count / Box Size / LED* param
G4 ok   LoadEhrFX: covered pixels 0 ; 4 empty slots; get Status A = 'Nothing loaded: transparent output. Point Model A (
G1 ok   LoadEhrWow: 490 params, none named Library* / Laser*
G6 ok   LoadEhrWow: no Mirror Room / Mirror Count / Box Size / LED* param
G4 ok   LoadEhrWow: covered pixels 0 ; 4 empty slots; get Status A = 'Nothing loaded: transparent output. Point Model A 
G1 ok   LoadEhrWowFX: 504 params, none named Library* / Laser*
G6 ok   LoadEhrWowFX: no Mirror Room / Mirror Count / Box Size / LED* param
G4 ok   LoadEhrWowFX: covered pixels 0 ; 4 empty slots; get Status A = 'Nothing loaded: transparent output. Point Model 
G2 ok   the only directory API: ModelLoader.cpp FolderIndex (textures next to the chosen model file):
G3 ok   no model / texture file inside build/LoadEhr*.bundle (8 files: binaries + Info.plist)
G5 ok   no Laser* / Library* sources, no library tools
G6 ok   no Mirror* source, no MIRROR program variant
G7 ok   no libtess2 / tesselator / SGI-B in src, third_party, CMakeLists.txt
G7 ok   no LGPL file in the tree (611 tracked files, submodules included)

drwxr-xr-x  31 <user>  staff    992 Oct  8 17:46 cue
drwxr-xr-x  31 <user>  staff    992 Oct  8 17:58 latency
drwxr-xr-x  15 <user>  staff    480 Oct  8 18:10 longshow
drwxr-xr-x  87 <user>  staff   2784 Oct  8 17:53 reverse
drwxr-xr-x  31 <user>  staff    992 Oct  8 17:45 sweep
drwxr-xr-x  11 <user>  staff    352 Oct  8 18:13 v_cue
drwxr-xr-x  27 <user>  staff    864 Oct  8 18:15 v_reverse
drwxr-xr-x  11 <user>  staff    352 Oct  8 18:12 v_sweep
drwxr-xr-x  16 <user>  staff    512 Oct  8 18:08 w3proof

LoadEhr W4 opacity 1.0
 C1 W4 Scrub Transport (Disconnected)
 C2 W4 Scrub BPM Loop (Disconnected)
 C3 W4 Scrub HAP (Disconnected)
 C4 W4 Scrub H264 (Disconnected)
 C5 W4 Loop Playhead (Disconnected)
 C6 W4 Reverse PingPong (Disconnected)
Arena PID 25852
```
END OF HANDOFF w4-scrub sha256:e63652b6 10465
