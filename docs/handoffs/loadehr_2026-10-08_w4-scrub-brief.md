```
LOADEHR W4 — A TIME SCRUBBER FOR THE PROJECTOR'S VIDEO: POSITION SLIDER (SCRUB / CUE), MOVING PLAYHEAD IF ARENA SHOWS IT, TIME READOUT, REVERSE SPEED — RUN TO FINISHED

MODE: build wave on LoadEhr (this session owns the folder). DXVKit submodule stays pinned (read-only). Commit + push at the end.
First: cp -p this brief from ~/claude-handoffs/ into docs/handoffs/.

AMRIT'S WORDS (2026-10-08, verbatim)
On the W3 clips: "8 - 9 identical as far as I can tell. 10 - 11 looks proper. 1 - 7 plays properly."
"Can we have a time scrubber on the clips, just like Arena has on their regular video clips?"

FACTS (checked; build from them)
1. Arena's own transport bar (the timeline under a video clip) does not exist for source / FFGL clips and its position is not passed to plugins: W28 measured FF_SET_TIME as a global millisecond clock (not clip time), a re-click reaches no plugin, and binding a param to clip_position was refused for FFGL source params; Resolume's forum has the same as an open feature request ("Missing Transport Position for Source Clips"). So the scrubber is OUR parameter, and Arena's parameter tools (drag, MIDI / OSC map, the parameter's own timeline animation with Play Once / Loop / BPM sync) are what make it behave like a transport.
2. Video today (CLAUDE.md PROJECTOR VIDEO + W3 DXV): one worker per file, decode window, Video Speed (0..4), Video Loop (Loop / Ping-Pong / Once), Video Restart, clock clk.video (double, wrapped), trigger restarts it. DXV / HAP: every frame is a key frame (seek = decode that packet). AVFoundation codecs (H.264 / HEVC / ProRes): reader restarted with a time range on a seek.
3. Param room: Wow 488, Wow FX 502 of 512. W25: widening a range is safe, shrinking clamps saved clips.

READ FIRST (all of it; extract before coding)
1. LoadEhr CLAUDE.md (PROJECTOR VIDEO, DXV, VIDEO ALPHA, W14 clocks, W25, W28 incl. the ARENA_ANIM timeline-animation method, the 512 cap, THE COMMONS), src/Video.h/.mm, Projector.cpp, the param shim (Map()), tools/arena_w3.py.
2. FFGL SDK: how a plugin raises a param VALUE event / updates its own displayed value (CFFGLPlugin param events, FF_EVENT flags; the same mechanism W-era SetParamVisibility uses) and how a TEXT param's display is updated. Then PROBE IT IN ARENA 7.28 before relying on it: does Arena move the slider when the plugin pushes a new value? does it record it into the clip? does it fight a user drag? Report the measured behaviour.
3. Apple: AVAssetReader timeRange restarts, AVPlayerItemVideoOutput / AVAssetImageGenerator (frame-accurate seeks with zero tolerance) for the AVFoundation codecs: pick what gives the lowest scrub latency without blocking the render thread.

IMPLEMENT (build the full set; Amrit prunes after playing)
1. Video Position (STD 0..1, Projector group, both Wow bundles): the clip position, frame-quantised.
2. Video Loop gains a 4th option "Scrub" (Loop / Ping-Pong / Once / Scrub; no new param): in Scrub the frame shown = Video Position exactly, no clock (drag it, map a fader, or animate it with Arena's parameter timeline = a transport bar). In Loop / Ping-Pong / Once a user change of Video Position CUES: the clock jumps there and keeps playing from that point (a value the plugin pushed itself is not a cue: tell the two apart).
3. PLAYHEAD: while playing (not Scrub), the plugin pushes its current position into Video Position so the slider moves like Arena's transport bar, IF the probe (READ FIRST 2) shows Arena displays it without fighting the user or spamming the clip; rate-limit the pushes (~15 / s). If Arena does not show it, leave it out and say so.
4. Video Time (TEXT, read-only, both Wow bundles): "mm:ss.f / mm:ss.f  frame n / N  <codec>" updated a few times a second (the Status-style display; if Arena does not refresh TEXT params live, say so and drop it).
5. Video Speed range -4..4 (widened from 0..4: reverse play; W25-safe). Ping-Pong / Loop / Once correct at negative speed; W14 clock wraps unchanged in meaning.
6. SEEK PATHS: DXV / HAP: direct packet decode of the wanted frame (already instant). AVFoundation: a scrub request cancels the current read and decodes the nearest frame; while it is pending the last frame stays (never block, never a black frame). Coalesce fast drags (latest position wins).
7. Counts: Wow 490, Wow FX 504 (<= 512). Names <= 16 chars. CLAUDE.md + param map updated; a Commons finding for the measured Arena behaviour of plugin-pushed param values (an FFGL / Arena host fact other builds will hit).

BUILD + TEST
1. Harness: Scrub sweeps 0 -> 1 -> 0 on a DXV, a HAP and an H.264 file: the frame shown = round(position x (N - 1)) every step, both directions; cue-while-playing lands on the cued frame and continues; reverse speed plays backwards in each loop mode; Once at negative speed holds frame 0.
2. Scrub latency per codec (DXV, HAP, ProRes, H.264, HEVC, 1080p and 4K): ms from a position change to that frame on screen, median / p95, and frames held while waiting. Arena PERF while dragging on the M3 (Wire closed; name the machine).
3. Playhead push in Arena: slider moves while playing, no stutter, user drag wins, saving the comp does not write a stream of automation (check the .avc after Amrit's next save is not required: inspect what Arena exposes over REST instead).
4. W14: 180-min Loop on a short clip at speed 1 and -1, exact frames; regress max 0; guards G1-G7 all bundles; W3 DXV proof still passes.
5. Install; ask Amrit to quit Arena fully (Arena > Quit Arena, cmd-Q) and relaunch; WAIT; check the PID changed.
6. CLIPS, layer "LoadEhr W4", untriggered, soft, selection parked on an empty slot:
   - DXV clip in Scrub with Video Position animated by Arena's parameter timeline (Play Once & Hold, Duration = the file's length) = transport-like;
   - the same in Scrub with a BPM-synced loop animation (Arena's BPM sync on the param);
   - a HAP clip and an H.264 clip in Scrub, position at 0.5, ready to drag;
   - a Loop clip with the moving playhead (if it works);
   - reverse speed (-1) Ping-Pong.
   Say exactly which column holds which clip. Do not touch earlier clips.
7. Commit + push. Handoff docs/handoffs/loadehr_2026-10-08_w4-scrub-results.md, cp -p to ~/claude-handoffs/: the probe findings (pushed values, TEXT refresh), what shipped / what was dropped and why, latency table, param counts, clip columns, guards, Amrit's answers from the session (incl. his W3 verdict above).

STANDING RULES
One Claude per folder. Cowork runs no git. Never trigger or activate clips; never kill Arena; do not ask Amrit to save the comp. Carnival mask forbidden. No library, no lasers, no mirror room. W14 clocks, W28 ms host clock, W23 alpha, W25 range clamp, 512 params, 16-char names, Apple GL 4.1. Licences MIT / BSD / Apache / zlib / ISC / Boost only. Dead code goes. Copies preserve timestamps. Anything Amrit must do: WHAT / HOW / WHERE.

STOP after the handoff. Report.
```
