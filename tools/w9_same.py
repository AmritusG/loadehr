#!/usr/bin/env python3
"""LoadEhr W9: the earlier waves' pictures unchanged after a W9 stage.

usage: tools/w9_same.py <out> [wire] [points] [strips] [scrub] [dxv <ref build>]
  wire    tools/w5_wire.py renders           vs verify/w8/wire_same (the W8 run of the W5 renders)
  points  tools/w6_points.py renders         vs verify/w8/points
  strips  tools/w6_trans.py strips           vs verify/w8/strips (+ the strips' own forward = backward column)
  scrub   tools/w4_scrub.py sweep            DXV 1080 + 4K: every frame shown in order (its own verdict)
  dxv     tools/w8_dxv.py same <ref build>   the projector's pictures byte-identical to the reference build
Every PNG pair is diffed with the harness `compare` (mean / max / pixels > 2) -> <out>/same.md"""
import os, re, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REF = os.path.join(ROOT, 'verify', 'w8')


def run(cmd):
    r = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
    return r.stdout + r.stderr


def compare(out, mine, ref, label):
    pngs = sorted(f for f in os.listdir(mine) if f.endswith('.png') and os.path.exists(os.path.join(ref, f)))
    script = os.path.join(out, 'cmp_%s.txt' % re.sub(r'\W+', '_', label))
    open(script, 'w').write('\n'.join('compare %s %s' % (os.path.join(ref, f), os.path.join(mine, f)) for f in pngs) + '\n')
    so = run([os.path.join(ROOT, 'build', 'loadehr_harness'), script])
    rows = re.findall(r'compare (\S+) vs (\S+): mean ([\d.]+) max (\d+) pixels>2 (\d+)', so)
    worst = max([int(r[3]) for r in rows] or [0])
    bad = [os.path.basename(r[1]) + ' max ' + r[3] for r in rows if int(r[3]) > 0]
    return '| %s | %d PNGs | worst max %d | %s |' % (label, len(rows), worst, ', '.join(bad) or 'all max 0')


def main():
    out, what = sys.argv[1], sys.argv[2:]
    os.makedirs(out, exist_ok=True)
    rows = ['| check | compared | worst | differing |', '|---|---|---|---|']
    if 'wire' in what:
        run(['python3', 'tools/w5_wire.py', 'renders', os.path.join(out, 'wire')])
        rows.append(compare(out, os.path.join(out, 'wire'), os.path.join(REF, 'wire_same'), 'W5 wire renders'))
        print(rows[-1], flush=True)
    if 'points' in what:
        run(['python3', 'tools/w6_points.py', 'renders', os.path.join(out, 'points')])
        rows.append(compare(out, os.path.join(out, 'points'), os.path.join(REF, 'points'), 'W6 points renders'))
        print(rows[-1], flush=True)
    if 'strips' in what:
        so = run(['python3', 'tools/w6_trans.py', 'strips', os.path.join(out, 'strips')])
        open(os.path.join(out, 'strips.txt'), 'w').write(so)
        fb = re.findall(r'\(worst (\d+)\)', so)
        rows.append(compare(out, os.path.join(out, 'strips'), os.path.join(REF, 'strips'), 'W6 / W7 transition strips'))
        rows.append('| strips forward = backward | %d rows | worst %s | |' % (len(fb), max(map(int, fb)) if fb else '?'))
        print('\n'.join(rows[-2:]), flush=True)
    if 'scrub' in what:
        gen = os.path.join(ROOT, 'test_media', 'gen', 'w4')
        files = [os.path.join(gen, f) for f in sorted(os.listdir(gen)) if 'dxv' in f]
        so = run(['python3', 'tools/w4_scrub.py', 'sweep', os.path.join(out, 'scrub')] + files)
        open(os.path.join(out, 'scrub.txt'), 'w').write(so)
        ok = re.findall(r'\| (\S+) \| (\d+) \| (\d+) \(want (\d+)\) \| (yes|no) \| (\d+) in between, (\d+) wrong', so)
        rows.append('| W4 scrub sweep (DXV) | %s | | %s |' % (len(ok), '; '.join('%s %s/%s %s, %s wrong' % (r[0], r[2], r[3], r[4], r[6]) for r in ok)))
        print(rows[-1], flush=True)
    if 'dxv' in what:
        ref = what[what.index('dxv') + 1]
        so = run(['python3', 'tools/w8_dxv.py', 'same', os.path.join(out, 'dxv'), ref])
        open(os.path.join(out, 'dxv.txt'), 'w').write(so)
        rows.append('| W8 DXV / video pictures vs %s | | | %s |' % (os.path.basename(ref.rstrip('/')), ' '.join(l.strip() for l in so.splitlines()[-16:])))
        print(rows[-1], flush=True)
    open(os.path.join(out, 'same.md'), 'w').write('\n'.join(rows) + '\n')


if __name__ == '__main__':
    main()
