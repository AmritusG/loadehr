#!/usr/bin/env python3
"""LoadEhr W6 Stage 2: the transition pack (Transition Style 5..21; W7: Shrinkwrap 6 / Fold 22 retired) in the harness.

usage: tools/w6_trans.py strips <out> [style ...]  every style, 9-step scrub strips t = 0 .. 1 forward, then the same steps backward,
                                                   on skull -> SciFi helmet and text -> skull (LoadEhr) and skull -> Crown 1918 (Wow):
                                                   forward vs backward per step (max difference: 0 = bit-identical), the ends vs the
                                                   plain models (t = 0 vs Model A alone, t = 1 vs Model B alone), the onset (t = 0.01 /
                                                   0.99 vs the plain models) -> <out>/strips.md + <out>/<pair>_<style>.png strips
       tools/w6_trans.py chain <out>               4 slots + Chain Loop: the D -> A hop mid-way for every style (per-slot data per hop)
       tools/w6_trans.py cost <out>                harness 1080p plugin / GPU ms mid-hop (t = 0.5) per style, skull -> helmet (Wow)
       tools/w6_trans.py waits <out>               4 slots, Transition Play Loop, Style Order Cycle round every style from a cold start:
                                                   the frame times (no frame waits on a load: the worst frame and where it falls)
       tools/w6_trans.py smoke <out>               the Smoke style played (Transition Play Once): the smoke's share of the frame over time
Clocks are zeroed before every shot (advance 0 zero), so a step renders the same whichever way the slider came."""
import os, re, subprocess, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import w5_wire as W  # noqa: E402

ROOT = W.ROOT
LIB = W.LIB
SKULL, HELMET = W.SKULL, W.HELMET
CROWN = LIB + '/Crowns/Crown 1918/scene.gltf'
STYLES = ['Dissolve', 'Shatter', 'Dust', 'Slice', 'Liquid', 'Points', '~Shrinkwrap', 'Wire', 'Voxel', 'Low Poly', 'Pieces', 'Implode', 'Twist', 'Melt',
          'Scan', 'Glitch', 'Ripple', 'Swarm', 'Smoke', 'Burn', 'Lava', 'Spin Swap', '~Fold']
PACK = [st for st in range(5, 23) if not STYLES[st].startswith('~')]  # W7: Shrinkwrap (6) and Fold (22) retired (tombstones)
WOW_ONLY = (17, 19, 20)
STEPS = [k / 8 for k in range(9)]
LOOK = ['set Rotate Y 25', 'set Rotate X -8', 'set Wire Glow 1', 'set Wire Color Mix 0']


def slot_a(m):
    if m == 'text':
        return ['set Model Source 1', 'mark', 'text Text LOADEHR', 'wait_loaded 60']
    return ['text Model_A ' + m, 'wait_loaded 60']


def pair_setup(bundle, a, b, size=(640, 360)):
    L = ['bundle %s/build/%s.bundle' % (ROOT, bundle), 'size %d %d' % size, 'instance'] + slot_a(a)
    return L + ['mark', 'text Model_B ' + b, 'wait_slots 1 60'] + LOOK + ['realtime 1.5']


def single(bundle, m, size=(640, 360)):
    return ['bundle %s/build/%s.bundle' % (ROOT, bundle), 'size %d %d' % size, 'instance'] + slot_a(m) + LOOK + ['realtime 1.5']


def shot(path):
    return ['advance 0 zero', 'frames 2', 'composite arena', 'shot ' + path]


PAIRS = [('skull_helmet', 'LoadEhr', SKULL, HELMET), ('text_skull', 'LoadEhr', 'text', SKULL), ('skull_crown', 'LoadEhrWow', SKULL, CROWN)]


def compare(so):
    return {(a, b): (float(mean), int(mx)) for a, b, mean, mx in re.findall(r'compare (\S+) vs (\S+): mean ([\d.]+) max (\d+)', so)}


def strips(out, only):
    os.makedirs(out, exist_ok=True)
    styles = [int(s) for s in only] if only else PACK
    rows = ['| pair | style | forward = backward (9 steps: max diff, worst) | t = 0 vs Model A alone | t = 1 vs Model B alone | onset t = 0.01 / 0.99 '
            'vs plain (mean, max) | strip |', '|---|---|---|---|---|---|---|']
    for tag, bundle, a, b in PAIRS:
        d = os.path.join(out, tag)
        os.makedirs(d, exist_ok=True)
        # the plain models: each alone in slot A (the same pose)
        L = single(bundle, a) + shot('%s/plainA.png' % d) + ['delete'] + single(bundle, b) + shot('%s/plainB.png' % d) + ['delete']
        for st in styles:
            # a fresh instance per style (the Smoke style's sim smoke lingers by design), warmed at mid-hop (the pair's per-slot data:
            # until it lands a pack hop dissolves); Smoke's strip is its mesh alone (Smoke Amount 0: the smoke is a live sim, `smoke`)
            L += pair_setup(bundle, a, b) + ['set Transition_Style %d' % st] + (['set Smoke Amount 0'] if st == 18 else [])
            L += ['set Transition 0.5', 'realtime 0.8']
            for i, t in enumerate(STEPS):
                L += ['set Transition %g' % t] + shot('%s/s%02d_f%d.png' % (d, st, i))
            for i, t in reversed(list(enumerate(STEPS))):
                L += ['set Transition %g' % t] + shot('%s/s%02d_b%d.png' % (d, st, i))
            for t, n in ((0.01, 'on0'), (0.99, 'on1')):
                L += ['set Transition %g' % t] + shot('%s/s%02d_%s.png' % (d, st, n))
            L += ['set Transition 0', 'delete']
            L += ['compare %s/s%02d_f%d.png %s/s%02d_b%d.png' % (d, st, i, d, st, i) for i in range(9)]
            L += ['compare %s/plainA.png %s/s%02d_f0.png' % (d, d, st), 'compare %s/plainB.png %s/s%02d_f8.png' % (d, d, st),
                  'compare %s/plainA.png %s/s%02d_on0.png' % (d, d, st), 'compare %s/plainB.png %s/s%02d_on1.png' % (d, d, st)]
        so, log = W.harness(L, d, 'strips')
        c = compare(so)
        for st in styles:
            fb = [c.get(('%s/s%02d_f%d.png' % (d, st, i), '%s/s%02d_b%d.png' % (d, st, i)), (-1, -1))[1] for i in range(9)]
            e0 = c.get(('%s/plainA.png' % d, '%s/s%02d_f0.png' % (d, st)), (-1, -1))[1]
            e1 = c.get(('%s/plainB.png' % d, '%s/s%02d_f8.png' % (d, st)), (-1, -1))[1]
            o0 = c.get(('%s/plainA.png' % d, '%s/s%02d_on0.png' % (d, st)), (-1, -1))
            o1 = c.get(('%s/plainB.png' % d, '%s/s%02d_on1.png' % (d, st)), (-1, -1))
            strip = '%s/%s_%02d_%s.png' % (out, tag, st, STYLES[st].lower().replace(' ', ''))
            subprocess.run(['montage'] + ['%s/s%02d_f%d.png' % (d, st, i) for i in range(9)] + ['-tile', '9x1', '-geometry', '256x144+1+1', strip],
                           capture_output=True)
            fall = ' (classic: dissolves)' if bundle != 'LoadEhrWow' and st in WOW_ONLY else ''
            rows.append('| %s | %d %s%s | %s (worst %d) | max %d | max %d | %.3f, %d / %.3f, %d | %s |' % (
                tag, st, STYLES[st], fall, ' '.join(map(str, fb)), max(fb), e0, e1, o0[0], o0[1], o1[0], o1[1], os.path.basename(strip)))
            print(rows[-1], flush=True)
    open(os.path.join(out, 'strips.md'), 'w').write('\n'.join(rows) + '\n')


def chain(out):
    os.makedirs(out, exist_ok=True)
    G = os.path.join(ROOT, 'test_models', 'gen')
    L = ['bundle %s/build/LoadEhrWow.bundle' % ROOT, 'size 640 360', 'instance', 'mark', 'text Model_A ' + SKULL, 'text Model_B ' + HELMET,
         'text Model_C ' + CROWN, 'text Model_D %s/knot/knot.obj' % G, 'wait_slots 4 90', 'set Chain Loop 1'] + LOOK + ['realtime 2']
    # 4 slots + Chain Loop = 4 hops: Transition 0.875 = the middle of hop 4 (D -> A)
    for st in PACK:
        L += ['set Transition_Style %d' % st, 'set Transition 0.875'] + shot('%s/da_%02d.png' % (out, st))
    so, log = W.harness(L, out, 'chain')
    subprocess.run(['montage'] + ['%s/da_%02d.png' % (out, st) for st in PACK] + ['-tile', '6x3', '-geometry', '320x180+1+1', '%s/chain_da.png' % out],
                   capture_output=True)
    print('chain: %s/chain_da.png (D -> A mid-hop, the pack styles)' % out)


def cost(out):
    os.makedirs(out, exist_ok=True)
    L = pair_setup('LoadEhrWow', SKULL, HELMET, (1920, 1080)) + ['set Wire Glow 0']
    cases = [('rest (Transition 0)', ['set Transition 0', 'set Transition_Style 0'])]
    cases += [('%d %s, t = 0.5' % (st, STYLES[st]), ['set Transition_Style %d' % st, 'set Transition 0.5']) for st in range(0, 23)
              if not STYLES[st].startswith('~')]
    for i, (c, cmd) in enumerate(cases):
        L += cmd + ['realtime 1.0', 'stats_reset', 'frames 120', 'stats C%d' % i]
    so, _ = W.harness(L, out, 'cost')
    rows = ['| case | plugin ms avg | GPU ms avg | GPU ms p99 |', '|---|---|---|---|']
    for i, (c, _) in enumerate(cases):
        m = re.search(r'stats C%d\s.*?plugin wall ms avg\s+([\d.]+).*?GPU ms avg\s+([\d.]+)\s+p99\s+([\d.]+)' % i, so)
        rows.append('| %s | %s | %s | %s |' % ((c,) + (m.groups() if m else ('?', '?', '?'))))
    print('\n'.join(rows))
    open(os.path.join(out, 'cost.md'), 'w').write('\n'.join(rows) + '\n')


def waits(out):
    os.makedirs(out, exist_ok=True)
    G = os.path.join(ROOT, 'test_models', 'gen')
    rows = []
    for bundle in ('LoadEhr', 'LoadEhrWow'):
        # Cycle from style 5: 4 hops per loop, so 5 loops visit every pack style at least once; Transition Time 0.5 s, Hold 0
        L = ['bundle %s/build/%s.bundle' % (ROOT, bundle), 'size 1280 720', 'instance', 'mark', 'text Model_A ' + SKULL, 'text Model_B ' + HELMET,
             'text Model_C ' + CROWN, 'text Model_D %s/knot/knot.obj' % G, 'wait_slots 4 90', 'set Chain Loop 1', 'set Style Order 1',
             'set Transition_Style 5', 'set Transition Time 0.5', 'set Transition Play 3', 'realtime 0.2', 'stats_reset', 'realtime 12', 'stats COLD',
             'stats_reset', 'realtime 12', 'stats WARM']
        so, log = W.harness(L, out, 'waits_' + bundle)
        for tag in ('COLD', 'WARM'):  # the first 12 s (each model's first draw prewarms its programs), then 12 s more
            m = re.search(r'stats %s\s.*' % tag, so)
            rows.append('%s: %s' % (bundle, m.group(0) if m else '?'))
            print(rows[-1], flush=True)
    open(os.path.join(out, 'waits.md'), 'w').write('\n'.join(rows) + '\n')


def smoke(out):
    os.makedirs(out, exist_ok=True)
    L = pair_setup('LoadEhrWow', SKULL, HELMET) + ['set Transition_Style 18', 'set Transition Time 3', 'set Transition Play 1']
    for k in range(10):
        L += ['realtime 0.5', 'composite arena', 'shot %s/smoke_%02d.png' % (out, k), 'shot_rgba %s/smoke_%02d_raw.png' % (out, k)]
    W.harness(L, out, 'smoke')
    rows = ['| s | coverage (raw alpha > 0) |', '|---|---|']
    for k in range(10):
        rows.append('| %.1f | %.4f |' % (0.5 * (k + 1), W.coverage('%s/smoke_%02d_raw.png' % (out, k))))
        os.remove('%s/smoke_%02d_raw.png' % (out, k))
    subprocess.run(['montage'] + ['%s/smoke_%02d.png' % (out, k) for k in range(10)] + ['-tile', '5x2', '-geometry', '320x180+1+1', '%s/smoke.png' % out],
                   capture_output=True)
    print('\n'.join(rows))
    open(os.path.join(out, 'smoke.md'), 'w').write('\n'.join(rows) + '\n')


if __name__ == '__main__':
    c, a = sys.argv[1], sys.argv[2:]
    {'strips': lambda: strips(a[0], a[1:]), 'chain': lambda: chain(a[0]), 'cost': lambda: cost(a[0]), 'waits': lambda: waits(a[0]),
     'smoke': lambda: smoke(a[0])}[c]()
