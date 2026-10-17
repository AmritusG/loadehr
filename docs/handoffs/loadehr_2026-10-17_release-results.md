project:    loadehr
pass:       release-1.0.0
STATUS:     COMPLETE
parent:     535cd28
result:     1aaa989 (tag v1.0.0)
session:    6d76d8dd-7b02-43ab-a345-b35e8d1c7c6a
written_at: 2026-10-17T20:10:00Z

# LoadEhr 1.0.0: first public release -- r2 (COMPLETE)

r2 follows loadehr_2026-10-17_release-results_BLOCKED.md; everything in it stands (READ FIRST findings, signing /
notarization / Gatekeeper receipts, deviations: PKG skipped for want of a Developer ID Installer identity). New since then:
- Amrit ran `gh release create v1.0.0 dist/LoadEhr-1.0.0.dmg --title "LoadEhr 1.0.0" --notes-file dist/notes.md`.
- RELEASE: https://github.com/AmritusG/loadehr/releases/tag/v1.0.0 (tag v1.0.0 on 1aaa989)
- Asset: LoadEhr-1.0.0.dmg, 9,760,406 bytes, sha256 8af8e2c099f87aefc62ed56f42ca9ae065a57033da5fce204eddd35caa2955f0.
- Logged-out checks pass: page 200, asset 302 to GitHub's asset host; the downloaded file's sha256 matches the local DMG
  and Gatekeeper accepts the download as "Notarized Developer ID".
- Still Amrit's call (optional): a real install on a second Mac (steps in the BLOCKED handoff), and a PKG once a
  Developer ID Installer certificate exists.

## RECEIPTS
```
$ curl -s -o /dev/null -w "%{http_code}" https://github.com/AmritusG/loadehr/releases/tag/v1.0.0
200
$ curl -s -o /dev/null -w "%{http_code} -> %{redirect_url}" https://github.com/AmritusG/loadehr/releases/download/v1.0.0/LoadEhr-1.0.0.dmg
302 -> https://release-assets.githubusercontent.com/github-production-release-asset/...
$ curl -sL -o <scratch>/LoadEhr-1.0.0.dmg <asset URL>; shasum -a 256
8af8e2c099f87aefc62ed56f42ca9ae065a57033da5fce204eddd35caa2955f0
$ spctl --assess --type open --context context:primary-signature -v <scratch>/LoadEhr-1.0.0.dmg
source=Notarized Developer ID          (scratch download deleted)
$ git log -1 (before this handoff): 535cd28 Release 1.0.0 handoff (BLOCKED: gh release create needs Amrit)
```
END OF HANDOFF release-1.0.0 sha256:5e20faf8 1716
