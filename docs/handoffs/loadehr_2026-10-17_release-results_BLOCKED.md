project:    loadehr
pass:       release-1.0.0
STATUS:     BLOCKED
parent:     1168515
result:     1aaa989 (tag v1.0.0)
session:    6d76d8dd-7b02-43ab-a345-b35e8d1c7c6a
written_at: 2026-10-17T20:07:06Z

# LoadEhr 1.0.0: first public release (BLOCKED at `gh release create`)

## Amrit's words (2026-10-17)
"Can we have the finished build in FFGL so a user can just drop it in or install it?"

## Where it stands
- DONE: Release build (universal), version 1.0.0, Developer ID signing + hardened runtime, each bundle notarized AND stapled,
  the DMG signed + notarized + stapled, Gatekeeper checks on quarantined copies, harness load test of those copies, README
  Install / Uninstall, commit 1aaa989 pushed, tag v1.0.0 pushed (e49f926 = the annotated tag object).
- SKIPPED: the PKG. No "Developer ID Installer" identity in the keychain (`security find-identity -v` lists only Developer ID
  Application + Apple Development). Not invented.
- BLOCKED: `gh release create` was refused by Claude Code's auto-mode classifier ("Create Public Surface"), as the brief
  foresaw. The release page and asset URLs therefore do not exist yet; their 200 / 302 check is owed.

## What Amrit must run (WHAT / HOW / WHERE)
- WHAT: publish the GitHub release with the DMG. WHERE: Terminal, or this Claude Code session with a leading `! `.
  HOW (one line):
  `cd ~/Documents/claude/ffgl/LoadEhr && gh release create v1.0.0 dist/LoadEhr-1.0.0.dmg --title "LoadEhr 1.0.0" --notes-file dist/notes.md`
  Success prints the release URL: https://github.com/AmritusG/loadehr/releases/tag/v1.0.0
- Then (optional, the last real check): on a second Mac (the M3 or the Studio), download the DMG from that page in Safari,
  open it, drag the four bundles into ~/Documents/Resolume Arena/Extra Effects, start Arena, add LoadEhr to a clip and
  point Model A at any .glb. Expected: no Gatekeeper prompt, the model renders. Remove the four bundles afterwards if
  that Mac should not keep them.
- Optional: an installer PKG needs a "Developer ID Installer" certificate (developer.apple.com > Certificates > + >
  Developer ID Installer); with it in the keychain the PKG is a short follow-up pass.

## Files (dist/, gitignored, NOT committed)
| File | Size | SHA-256 |
|---|---|---|
| LoadEhr-1.0.0.dmg | 9,760,406 bytes | 8af8e2c099f87aefc62ed56f42ca9ae065a57033da5fce204eddd35caa2955f0 |
DMG contents: LoadEhr.bundle, LoadEhrFX.bundle, LoadEhrWow.bundle, LoadEhrWowFX.bundle, Install.txt, LICENSE.txt.
No Finder alias: an alias to ~/Documents/Resolume Arena/Extra Effects points at this Mac's user path and would not resolve
on another Mac. dist/notes.md = the release notes. dist/build and dist/stage = the release build + the signed bundles.

## READ FIRST: what was found
- Plugin folders (Resolume's Directory List, resolume.com/support/en/directory-list): third-party plugins go in
  ~/Documents/Resolume Arena/Extra Effects ("3rd party plugins can be installed here and will be loaded automatically").
  Forum (Resolume moderators): more folders can be added under Preferences > Video. No system-wide folder is documented;
  the app's own Plugins folder holds only the built-in plugins. Duplicates: Resolume documents nothing, so Install.txt and
  README say "one folder only".
- Stapling (Apple DTS on the developer forums): notarize and staple the outermost container (DMG / PKG); a ZIP cannot be
  stapled; a container's ticket does not staple onto items copied out of it. So I notarized the four bundles too (one zip
  submission) and stapled each bundle: a bundle dragged out of the DMG carries its own ticket and works offline.
  `stapler staple` on these .bundles worked (ticket in Contents/CodeResources).
- Entitlements: none. A plugin needs no hardened-runtime entitlement; loading third-party code is the HOST's concern
  (Arena's library-validation setting). Signed with --options runtime, no entitlements file.
- Glance scripts/notarize.sh (read, not edited): `notarytool submit --keychain-profile NotaryBuilds --wait`, then stapler
  + spctl. The profile was already in the keychain (`notarytool history` answered), so no credential step was needed and no
  password went anywhere.

## Deviations
- PKG skipped (no Installer identity), as the brief said.
- Extra notarization of the bare bundles (beyond the DMG), see READ FIRST. Two submissions, both Accepted.
- `spctl --assess --type execute` REJECTS the bundles ("the code is valid but does not seem to be an app"): that is the
  app policy and is expected for any plugin; the plugin-relevant check (`--type open --context context:primary-signature`)
  ACCEPTS them as "Notarized Developer ID". The stronger proof: the quarantined copies load and render in the offline
  harness (a real dlopen through macOS's quarantine path), see receipts.
- The FFGL info block's own plugin version (in code) was not touched (packaging only); Info.plist says 1.0.0.

## RECEIPTS
```
$ git log -1 --format='%H %d'
1aaa9898f0900287a710628d89945995ed985cf1  (HEAD -> main, tag: v1.0.0, origin/main, origin/HEAD)
$ git ls-remote --tags origin v1.0.0
e49f926e40ce931e9d75728f453928528579ce88	refs/tags/v1.0.0
$ security find-identity -v -p codesigning   (installer identities: none)
  1) D2683095DE5421C7A292F448BAF81DB8BAA0141E "Developer ID Application: ... (3A3L2C6DFB)"
  2) A74F14C8A19A679BEF5E333E6880A719BEEC5485 "Apple Development: ... (LC8R26DZFP)"
$ lipo -archs (each bundle)          -> x86_64 arm64   (all four)
$ find <bundle> -type f | wc -l      -> 2 files each before signing (binary + Info.plist): no model / texture (G3)
$ otool -L (all four, both archs): only /System/Library/Frameworks/{AVFoundation,CoreFoundation,CoreGraphics,CoreMedia,
  CoreText,CoreVideo,Foundation,IOSurface,OpenGL,Vision(FX only)} + /usr/lib/{libSystem.B,libc++.1,libobjc.A}.dylib
$ strings | grep -cE '/opt/homebrew|/usr/local/|/Users/'   -> 0 0 0 0
$ codesign --force --options runtime --timestamp --sign D2683095...0141E dist/stage/<b>.bundle
$ codesign --verify --deep --strict --verbose=2   (each)
dist/stage/LoadEhr.bundle: valid on disk / satisfies its Designated Requirement      (same for FX, Wow, WowFX)
Identifier=com.amritusg.loadehr  flags=0x10000(runtime)  Timestamp=17 Oct 2026 at 22:02:47  TeamIdentifier=3A3L2C6DFB
$ xcrun notarytool submit bundles.zip --keychain-profile NotaryBuilds --wait
  id: e40d4d3f-9d36-4be0-9e33-fea529097727   status: Accepted   (log: issues None, 16 tickets)
$ xcrun stapler staple / validate <b>.bundle   -> "The staple and validate action worked!" x4, "The validate action worked!" x4
$ hdiutil create -volname "LoadEhr 1.0.0" -format UDZO ... ; codesign --timestamp --sign ... dist/LoadEhr-1.0.0.dmg
dist/LoadEhr-1.0.0.dmg: valid on disk / satisfies its Designated Requirement
$ xcrun notarytool submit dist/LoadEhr-1.0.0.dmg --keychain-profile NotaryBuilds --wait
  id: 435816cc-e30e-4192-9c13-20dc37937669   status: Accepted
$ xcrun stapler staple / validate dist/LoadEhr-1.0.0.dmg -> worked / worked
$ spctl --assess --type open --context context:primary-signature -vv dist/LoadEhr-1.0.0.dmg
dist/LoadEhr-1.0.0.dmg: accepted   source=Notarized Developer ID   origin=Developer ID Application: ... (3A3L2C6DFB)
$ ls -la dist/LoadEhr-1.0.0.dmg ; shasum -a 256
-rw-r--r--@ 1 <user>  staff  9760406 Oct 17 22:04 dist/LoadEhr-1.0.0.dmg
8af8e2c099f87aefc62ed56f42ca9ae065a57033da5fce204eddd35caa2955f0  dist/LoadEhr-1.0.0.dmg
--- Stage 3: a stranger's download (scratch copies; quarantine 0083;...;Safari;<uuid> on the DMG and on every copied file)
$ spctl --assess --type open --context context:primary-signature <scratch>/LoadEhr-1.0.0.dmg -> accepted, Notarized Developer ID
$ (mounted read-only, -nobrowse; bundles ditto'd out, quarantined) per bundle:
  codesign --verify --deep --strict -> ok ; stapler validate -> worked ;
  spctl --type execute -> rejected (not an app: expected) ; spctl --type open primary-signature -> accepted, Notarized Developer ID
$ loadehr_harness on the quarantined copies (fresh instance, then Model A = Human Skull by path):
loaded LoadEhr id LEhr type 1, 487 params        Status A 'Nothing loaded: transparent output...'  SWAP 31016 tris
loaded LoadEhr FX id LEfx type 0, 508 params     same
loaded LoadEhr Wow id LEwo type 1, 497 params    same
loaded LoadEhr Wow FX id LEwf type 0, 510 params same   (render looked at: the skull, over the checker)
$ gh release create v1.0.0 dist/LoadEhr-1.0.0.dmg --title "LoadEhr 1.0.0" --notes-file dist/notes.md
DENIED by the Claude Code auto mode classifier: [Create Public Surface]   (not run)
```
Scratch (quarantined copies, mount, renders) cleared; dist/bundles.zip and dist/dmgroot removed. Arena, its composition and
the installed dev bundles were not touched. Owed after Amrit's line: release page 200 + asset URL 302 (logged out).
END OF HANDOFF release-1.0.0 sha256:4a0b9bec 8576
