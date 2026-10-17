project:    loadehr
pass:       go-public
STATUS:     BLOCKED
parent:     5d906eb
result:     NONE
session:    6d76d8dd-7b02-43ab-a345-b35e8d1c7c6a
written_at: 2026-10-17T19:14:41Z

# LoadEhr: go public -- r3 (LoadEhr is PUBLIC; DXVKit still private)

r3 follows r2 (loadehr_2026-10-17_go-public-results_BLOCKED-r2.md); everything in r2 stands. New since r2:
- Amrit ran the flip himself in this session: `gh repo edit AmritusG/loadehr --visibility public
  --accept-visibility-change-consequences` (a first try failed only on a trailing period after the flag).
- https://github.com/AmritusG/loadehr is PUBLIC, logged-out HTTP 200.
- DXVKit is still private (404 logged out), as reported in r2. So a credential-free `git clone --recurse-submodules`
  clones LoadEhr (HEAD 5d906eb) but fails on third_party/dxvkit: LoadEhr cannot be built from a public clone yet.
- STILL BLOCKED on one thing: the codecs Cowork scrubs DXVKit (list in r2, Stage 4) and flips it
  (`gh repo edit AmritusG/dxvkit --visibility public --accept-visibility-change-consequences`). Then re-run the clone
  check below; expected: exit 0, third_party/dxvkit populated.

## RECEIPTS
```
$ gh repo view AmritusG/loadehr --json visibility,url
PUBLIC https://github.com/AmritusG/loadehr
$ curl -s -o /dev/null -w "%{http_code}" https://github.com/AmritusG/loadehr   -> 200
$ curl -s -o /dev/null -w "%{http_code}" https://github.com/AmritusG/dxvkit    -> 404
$ GIT_TERMINAL_PROMPT=0 GIT_CONFIG_GLOBAL=/dev/null git -c credential.helper= clone --recurse-submodules https://github.com/AmritusG/loadehr.git <scratch>/clonetest
fatal: could not read Username for 'https://github.com': terminal prompts disabled
Failed to clone 'third_party/dxvkit' a second time, aborting
exit 1   (LoadEhr itself cloned: HEAD 5d906eb; third_party/dxvkit empty; scratch clone deleted)
```
END OF HANDOFF go-public sha256:56f4f814 1655
