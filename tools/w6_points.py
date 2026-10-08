#!/usr/bin/env python3
"""LoadEhr W6 Stage 1: Points Only / Points + Wire (Wire Mode 4 / 5), Point Source, Point Twinkle, in the harness.

usage: tools/w6_points.py renders <out>    every Point Source (Vertices / Feature Vertices / Surface 25k / 100k / 400k) as Points Only
                                          (Hidden) on the skull, the SciFi helmet, text "LOADEHR" and our SVG emblem, + Points + Wire and
                                          X-Ray, over the arena background -> <out>/*.png + renders.md (coverage = raw alpha > 0; the point
                                          count = the worker's POINTS log line)
       tools/w6_points.py follow <out>     the points follow Deform (Twist), Letters (Letter Wave), the shatter flight (vs Wire Only at
                                          the same flight time: the same spread), clones, the morph chain mid-hop; Point Twinkle moves
                                          -> follow.md
       tools/w6_points.py cost <out>       harness 1080p plugin / GPU ms per source, X-Ray, Points + Wire, Glow / Bloom -> cost.md
       tools/w6_points.py wire_same <out>  the W5 renders (tools/w5_wire.py renders) again: max difference vs verify/w5/renders -> wire_same.md"""
import os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import w5_wire as W  # noqa: E402  (png, coverage, harness, load, the models)

SOURCES = ['Vertices', 'Feature Vertices', 'Surface 25k', 'Surface 100k', 'Surface 400k']
LOOK = [('Wire Color', 0.5), ('Wire Sat', 1), ('Wire Bri', 1), ('Wire Color Mix', 1), ('Wire Glow', 1)]


def pts(mode=4, source=3, back=0, extra=()):
    return ['set Wire Mode %d' % mode, 'set Point Source %d' % source, 'set Wire Back %d' % back] + ['set %s %g' % kv for kv in LOOK + list(extra)]


def counts(log):
    return [(s, int(n)) for s, n in re.findall(r'POINTS ([A-Za-z0-9 ]+?): (\d+) points', log)]


def bbox(path):
    w, h, px = W.png(path)
    xs = [i % w for i in range(w * h) if px[i * 4 + 3] > 0]
    ys = [i // w for i in range(w * h) if px[i * 4 + 3] > 0]
    return (min(xs), min(ys), max(xs), max(ys)) if xs else None


def renders(out):
    os.makedirs(out, exist_ok=True)
    rows = ['| model | case | points | coverage (raw alpha > 0) | shot |', '|---|---|---|---|---|']
    for name, m in W.MODELS:
        L = W.load(m) + ['realtime 1.5']
        shots = []
        for s in range(5):
            tag = '%s_points_%s' % (name, SOURCES[s].lower().replace(' ', ''))
            L += pts(4, s) + ['realtime 0.6', 'composite arena', 'shot %s/%s.png' % (out, tag), 'shot_rgba %s/%s_raw.png' % (out, tag)]
            shots.append(('Points Only, %s' % SOURCES[s], tag, SOURCES[s]))
        for case, cmd, src in (('Points + Wire, Feature Vertices, Feature Edges', pts(5, 1, extra=(('Wire Edges', 1),)), 'Feature Vertices'),
                               ('Points Only X-Ray, Surface 100k', pts(4, 3, 1), 'Surface 100k')):
            tag = '%s_%s' % (name, 'pw' if 'Wire,' in case else 'xray')
            L += ['set Wire Edges 0'] + cmd + ['realtime 0.6', 'composite arena', 'shot %s/%s.png' % (out, tag), 'shot_rgba %s/%s_raw.png' % (out, tag)]
            shots.append((case, tag, src))
        so, log = W.harness(L, out, 'r_' + name)
        n = dict(counts(log))
        for case, tag, src in shots:
            rows.append('| %s | %s | %s | %.4f | %s.png |' % (name, case, n.get(src, '?'), W.coverage('%s/%s_raw.png' % (out, tag)), tag))
            os.remove('%s/%s_raw.png' % (out, tag))
        print('\n'.join(rows[-len(shots):]), flush=True)
    open(os.path.join(out, 'renders.md'), 'w').write('\n'.join(rows) + '\n')


def follow(out):
    os.makedirs(out, exist_ok=True)
    rows = ['| combination | coverage | bbox (raw, px) | shot |', '|---|---|---|---|']
    P = pts(4, 3, extra=(('Wire Glow', 2),))
    runs = [
        ('rest_text', W.load('text') + P + ['realtime 0.8']),
        ('deform_twist', W.load('text') + P + ['set Twist 3', 'realtime 0.8']),
        ('rest_text_classic', W.load('text', bundle='LoadEhr') + P + ['realtime 0.8']),  # the Letters group is classic-only
        ('letters_wave', W.load('text', bundle='LoadEhr') + P + ['set Letter Wave 0.8', 'realtime 0.8']),
        ('shatter_points', W.load('text') + P + ['set Shatter Mode 1', 'realtime 0.5', 'pulse Shatter', 'realtime 0.9']),
        ('shatter_wire', W.load('text') + W.wire(2) + ['set Shatter Mode 1', 'realtime 0.5', 'pulse Shatter', 'realtime 0.9']),
        ('clones', W.load(W.SKULL, bundle='LoadEhr') + P + ['set Clone Layout 1', 'set Clone Count 6', 'set Clone Spacing 3', 'set Distance 9', 'realtime 1.5']),
        ('chain_midhop', W.load(W.SKULL) + ['mark', 'text Model_B ' + W.HELMET, 'wait_slots 1 60'] + P + ['set Transition 0.5', 'set Transition Style 0', 'realtime 1.5']),
        ('shatter_skull', W.load(W.SKULL) + P + ['realtime 0.5', 'pulse Shatter', 'realtime 0.7']),
    ]
    for tag, L in runs:
        W.harness(L + ['frames 2', 'composite arena', 'shot %s/%s.png' % (out, tag), 'shot_rgba %s/%s_raw.png' % (out, tag)], out, tag)
        rows.append('| %s | %.4f | %s | %s.png |' % (tag, W.coverage('%s/%s_raw.png' % (out, tag)), bbox('%s/%s_raw.png' % (out, tag)), tag))
        os.remove('%s/%s_raw.png' % (out, tag))
        print(rows[-1], flush=True)
    # Point Twinkle: the picture changes frame to frame with it (and not without it), the skull held still
    L = W.load(W.SKULL) + pts(4, 3) + ['realtime 1', 'set Point Twinkle 0', 'realtime 0.3', 'diffs 30', 'set Point Twinkle 1', 'realtime 0.3', 'diffs 30']
    so, _ = W.harness(L, out, 'twinkle')
    d = [list(map(float, l.split()[1:])) for l in so.splitlines() if l.startswith('diffs:')]
    rows += ['', 'Point Twinkle 0 -> 1 (skull still, Surface 100k): mean per-frame change %.4f -> %.4f' % (sum(d[0]) / len(d[0]), sum(d[1]) / len(d[1])) if len(d) == 2 else 'twinkle: ?']
    print(rows[-1])
    open(os.path.join(out, 'follow.md'), 'w').write('\n'.join(rows) + '\n')


def cost(out):
    os.makedirs(out, exist_ok=True)
    cases = [('solid (Wire Mode Off)', ['set Wire Mode 0'])]
    cases += [('Points Only, %s' % s, pts(4, i, extra=(('Wire Glow', 0),))) for i, s in enumerate(SOURCES)]
    cases += [('Points Only X-Ray, Surface 100k', pts(4, 3, 1, (('Wire Glow', 0),))),
              ('Points + Wire, Surface 100k', pts(5, 3, extra=(('Wire Glow', 0),))),
              ('Points Only 100k + Glow 3', pts(4, 3, extra=(('Wire Glow', 3),))),
              ('Points Only 100k + Glow 3 + Bloom 3', pts(4, 3, extra=(('Wire Glow', 3), ('Wire Bloom', 3)))),
              ('Points Only 400k + Twinkle 1', pts(4, 4, extra=(('Wire Glow', 0), ('Point Twinkle', 1))))]
    rows = ['| case | %s |' % ' | '.join('%s plugin ms avg / GPU ms avg' % n for n, _ in W.MODELS[:2]), '|---|---|---|']
    res = {}
    for name, m in W.MODELS[:2]:
        L = W.load(m, (1920, 1080)) + ['realtime 1.5']
        for i, (c, w) in enumerate(cases):
            L += ['set Wire Bloom 0', 'set Point Twinkle 0'] + w + ['realtime 0.8', 'stats_reset', 'frames 120', 'stats C%d' % i]
        so, _ = W.harness(L, out, 'cost_' + name)
        for i in range(len(cases)):
            m2 = re.search(r'stats C%d\s.*?plugin wall ms avg\s+([\d.]+).*?GPU ms avg\s+([\d.]+)' % i, so)
            res[(name, i)] = '%s / %s' % m2.groups() if m2 else '?'
    for i, (c, _) in enumerate(cases):
        rows.append('| %s | %s | %s |' % (c, res[('skull', i)], res[('helmet', i)]))
    print('\n'.join(rows))
    open(os.path.join(out, 'cost.md'), 'w').write('\n'.join(rows) + '\n')


def wire_same(out):
    os.makedirs(out, exist_ok=True)
    W.renders(out)
    ref = os.path.join(W.ROOT, 'verify', 'w5', 'renders')
    rows = ['| shot | max difference vs W5 | pixels > 2 |', '|---|---|---|']
    worst = 0
    for f in sorted(os.listdir(out)):
        if not f.endswith('.png') or not os.path.exists(os.path.join(ref, f)):
            continue
        w, h, a = W.png(os.path.join(ref, f))
        _, _, b = W.png(os.path.join(out, f))
        d = [abs(x - y) for x, y in zip(a, b)]
        mx, n2 = max(d), sum(1 for i in range(0, len(d), 4) if max(d[i:i + 4]) > 2)
        worst = max(worst, mx)
        rows.append('| %s | %d | %d |' % (f, mx, n2))
    rows.append('')
    rows.append('worst: %d over %d shots' % (worst, len(rows) - 3))
    print('\n'.join(rows[-1:]))
    open(os.path.join(out, 'wire_same.md'), 'w').write('\n'.join(rows) + '\n')


if __name__ == '__main__':
    {'renders': renders, 'follow': follow, 'cost': cost, 'wire_same': wire_same}[sys.argv[1]](sys.argv[2])
