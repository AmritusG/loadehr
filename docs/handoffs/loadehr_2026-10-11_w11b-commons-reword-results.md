project:    LoadEhr
pass:       w11b-commons-reword
STATUS:     COMPLETE
parent:     6a145eb
result:     commons f6ba061
session:    7fb29fc7-1678-4bc4-ba23-11a2293a5935
written_at: 2026-10-11T10:02:20Z

# LoadEhr W11b: Commons LOADEHR-024 reworded (text only)

## Your instructions (verbatim, 2026-10-11)
- "Add that to Commons too. ... only build, and leave built clips in ARENA alone (on disk, when settled, clear images, not videos)"
- then: "I see no point in using videos unless needed to project inside the plugin, but since that has passed, all videos and their images can be swept away"

## What shipped
- Commons: the Commons PROTOCOL forbids editing a finding ("A finding later shown wrong is not deleted. Write the correction with
  supersedes:"), so the reword is a NEW finding **LOADEHR-025** with `supersedes: LOADEHR-024`; the index strikes 024. New text:
  in Arena a build only builds and leaves every built clip and layer alone (Amrit deletes the comp if and when he wants). On disk,
  once a wave is settled, clear proof renders AND generated test videos and their frames; keep only what a still-running test or
  the plugin needs (pinned regression references, a source a current test loads). Regenerable test media is rebuilt on demand by
  its make script, never kept "just in case". Contract proposal 8.1c reworded the same way.
- Project memory `arena-is-build-and-test-only.md` + its MEMORY.md line: same wording, now points at LOADEHR-025.
- LoadEhr CLAUDE.md: had no "keep videos" wording (grep), so unchanged.
- Nothing else touched; no Arena.

## Deviation
- New finding instead of an in-place edit of 024 (Commons PROTOCOL, see above).

## RECEIPTS
```
$ git -C _Commons log -1 ; status
f6ba0610182b33832e1bd1ca1166ea1cc41b74e6 commons: sync findings
## main...origin/main
$ grep "keep the videos|keep videos" CLAUDE.md memory/ LOADEHR-025 CONTRACT.md
exit 1 (1 = no match)
```
END OF HANDOFF w11b-commons-reword sha256:79be1eaf 1706
