# LOADEHR — COWORK TAKEOVER (from the MaskEhr Cowork, 2026-10-07 19:45)

You are now the Cowork for **LoadEhr**. The MaskEhr Cowork has handed it to you to keep its own thread clean.
All LoadEhr review, briefs and TL;DRs happen here. LoadEhr also has its OWN Claude Code session, which is not MaskEhr's.

## What LoadEhr is
A new FFGL plugin for Resolume Arena, spun off from MaskEhr. Amrit's words (2026-10-07, verbatim):
"I want a version of MaskEhr called LoadEhr that has basically the same features, no lasers though (might prune more) and that does NOT come with any library and does not HAVE a library. Only use two import for morphing between loaded 3D models, or, add more loaders to have more morphing capabilities."
- **Home:** folder ~/Documents/claude/ffgl/LoadEhr, private repo AmritusG/loadehr (main).
  Handoffs are named loadehr_<date>_<slug>.md and live in the repo's docs/handoffs/, with a cp -p copy in ~/claude-handoffs/.
- **HARD RULES:**
  - NO LIBRARY: no folder scan, no list, no bundled or default model. Models only come from FILE params. Nothing loaded means a transparent output.
  - NO LASERS: those live in Scotty, which has its own Cowork.
  - The carnival mask (MaskEhr/carnival_mask) is restricted ("for his use on show only"). It is never copied into LoadEhr and never committed.
- **4 model slots (A to D)** that morph in a chain. This was Cowork's pick under Amrit's "add, then prune" rule; he may cut it to 2.
  - Transition walks A → B → C → D, with Chain Loop back to A.
  - Style Order: Fixed / Cycle / Random.
  - Transition Play: Once / Ping-Pong / Loop, with a time per hop and a hold time.
- "might prune more": expect him to cut features after he plays with it.

## State right now
1. **W1 brief WRITTEN, not yet run.** Full text: ~/claude-handoffs/loadehr_2026-10-07_w1-kickoff.md.
   A copy is in MaskEhr/docs/handoffs/; CC copies it into LoadEhr's docs/handoffs/ first.
   It covers:
   - a new repo copied from MaskEhr;
   - lasers and the library removed;
   - 4 slots and the morph chain;
   - the fewest bundles that fit 512 params each;
   - parity against MaskEhr for the non-laser looks;
   - guard tests and grep receipts;
   - an Arena layer "LoadEhr" with 3 parity clips and 4 morph-chain clips, untriggered.
2. Continuation line for the NEW LoadEhr CC session:
   `Read /Users/amritrosell/Documents/claude/ffgl/MaskEhr/docs/handoffs/loadehr_2026-10-07_w1-kickoff.md and execute it.`
3. **Your next job:** when Amrit posts the W1 results (loadehr_2026-10-07_w1-results.md):
   - Read the whole file.
   - Check the no-library and no-laser proof: grep receipts, the guard tests, and that the param list has no Library* or Laser*.
   - Check how the 4-slot chain handles parts, the eye lights and the smoke obstacle as models change.
   - Give him a few lines of TL;DR in plain language.
   - Then either brief the next wave or end with no continuation line.
   - Open flag from the brief: libtess2 (text/logo extrusion) is SGI-B, which is not on his licence list. CC proposes a replacement or asks him. Put it to him as ONE plain question if it comes back.

## Folders this Cowork needs (ask Amrit once, if not already connected)
- ~/Documents/claude/ffgl/LoadEhr: READ ONLY. Its CC owns it, and one Claude per folder.
- ~/claude-handoffs: write briefs here, and also into LoadEhr/docs/handoffs if the folder is writable for you. If it is not, CC copies them in.
- ~/Documents/claude/ffgl/MaskEhr: READ ONLY reference (its CLAUDE.md is the full map). Never write there again.
- ~/Documents/Resolume Arena: only to look at the comp or the bundles.

## How Amrit works (keep it exactly like this)
- **Briefs:**
  - A brief is a FILE: one plain-text code-fenced block, CAPS labels and numbered lists.
  - Order: task + mode → AMRIT'S WORDS → READ FIRST → IMPLEMENT → BUILD + TEST → STANDING RULES → STOP. Report.
  - Write it to the handoffs folder and cp -p it to ~/claude-handoffs/.
  - End the chat with the bare continuation line in its own code block: `Read <full path> and execute it.`
  - No continuation line when the work is done with nothing queued.
  - "Decide with evidence whether to keep X" went wrong in Scotty W1. Say plainly what goes and what stays.
- **Chat:** short, short, short. A few lines of TL;DR in plain language, with no wave or code jargon.
- **Git:** Cowork runs NO git at all on his tree (not even status). CC commits and pushes when a wave is done.
- **Arena clips:**
  - Never trigger or activate clips. Never kill Arena. Never ask him to save the comp.
  - Clips are built untriggered with soft motion ("Soooooft I want, Crazy as max MAYBE"). Say which slots they are in.
  - "Change the dials, not the clip." Test clips are disposable.
  - To get a new bundle loaded: ask him to quit Arena fully (Arena > Quit Arena, cmd-Q) and relaunch, then check that the PID changed.
- **Building:**
  - ADD, then prune: build the full knob set and let him cut. Levers get headroom past "correct".
  - Dead code goes.
  - Copies preserve timestamps (cp -p / rsync -a).
  - Licences: MIT / BSD / Apache / zlib / ISC / Boost only. No GPL / AGPL.
- **His answers:** answers he gives inside CC do not reach you. CC must record his answers in its handoff.
  Check the newest handoff before asking him anything.
- **Things he must do:** written as WHAT / HOW / WHERE, with the exact on-screen labels.
- **Hard-won technical rules** (all in MaskEhr CLAUDE.md, carried into LoadEhr's):
  - W23 alpha contract (API 2.3, premultiplied).
  - W14 clocks: double precision, wrapped.
  - W28: Arena's host time is in MILLISECONDS.
  - W25: Arena clamps stored values when a range shrinks.
  - 512 params per plugin, 16-char names.
  - Colours use HSBA quartets (Arena's colour picker).
  - Apple GL 4.1 (no compute / geometry shaders, 16 samplers).
  - A clip re-click reaches no plugin, so restarts are done Arena-side with a timeline animation.
  - Arena's FPS readout floors at the display rate, and GL timestamps are 0 on Apple, so use the PERF instrument.

## Siblings (not yours)
- MaskEhr: its own Cowork + CC. Parked there: M5 at about 40 fps (not diagnosed).
- Scotty (lasers): its own Cowork + CC.
- Fanous (lantern): its own sessions.
