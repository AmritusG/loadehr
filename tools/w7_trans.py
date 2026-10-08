#!/usr/bin/env python3
"""LoadEhr W7: the transition review (Twist / Melt glitch, Twist / Melt reach, Trans Sparkle, Lava on the skull, Fold + Shrinkwrap out).

usage: tools/w7_trans.py scrub <out> <style> [pair] [extra set cmds ...]
          the hop scrubbed t 0 -> 1 -> 0 in 400 steps a way (one frame each, Spin Y 0): per step mean / max change + pixels changed > 32
          -> <out>/scrub_<pair>_<style>.txt; prints the worst steps and the hop's median change"""
import os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import w5_wire as W  # noqa: E402

ROOT, LIB = W.ROOT, W.LIB
M = {'skull': W.SKULL, 'helmet': W.HELMET, 'crown': LIB + '/Crowns/Crown 1918/scene.gltf', 'ram': LIB + '/Skulls/Ram Skull/scene.gltf',
     'ganesha': LIB + '/Masks/Ganesha Mask/scene.gltf', 'nefertiti': LIB + '/Heads/Nefertiti/scene.gltf', 'deer': LIB + '/Skulls/Deer Skull/scene.gltf',
     'text': 'text'}
assert not any('arnival' in p for p in M.values())  # HARD RULE 3
STYLES = ['Dissolve', 'Shatter', 'Dust', 'Slice', 'Liquid', 'Points', 'Shrinkwrap', 'Wire', 'Voxel', 'Low Poly', 'Pieces', 'Implode', 'Twist', 'Melt',
          'Scan', 'Glitch', 'Ripple', 'Swarm', 'Smoke', 'Burn', 'Lava', 'Spin Swap', 'Fold']
CLIP = ['set Key Intensity 0.8', 'set Spin Y 0']  # the W6 clips' look without the spin


def slot_a(m):
    if m == 'text':
        return ['set Model Source 1', 'mark', 'text Text LOADEHR', 'wait_loaded 60']
    return ['text Model_A ' + M[m], 'wait_loaded 60']


def pair_setup(bundle, a, b, size=(640, 360)):
    L = ['bundle %s/build/%s.bundle' % (ROOT, bundle), 'size %d %d' % size, 'instance'] + slot_a(a)
    return L + ['mark', 'text Model_B ' + M[b], 'wait_slots 1 60'] + CLIP + ['realtime 1.0']


def parse_scrub(so):
    return [(float(v), float(c), int(m), int(b)) for v, c, m, b in re.findall(r'scrub ([\d.]+) change ([\d.]+) max (\d+) big (\d+)', so)]


def scrub(out, style, pair='ram_crown', extra=(), bundle='LoadEhrWow', steps=400):
    os.makedirs(out, exist_ok=True)
    a, b = pair.split('_')
    st = STYLES.index(style) if not str(style).isdigit() else int(style)
    L = pair_setup(bundle, a, b) + ['set Transition_Style %d' % st] + list(extra) + ['set Transition 0.5', 'realtime 0.8', 'set Transition 0', 'frames 3']
    L += ['scrub 0 1 %d Transition' % steps, 'scrub 1 0 %d Transition' % steps]
    so, log = W.harness(L, out, 'scrub_%s_%s' % (pair, STYLES[st].replace(' ', '')))
    r = parse_scrub(so)
    open(os.path.join(out, 'scrub_%s_%s.txt' % (pair, STYLES[st].replace(' ', ''))), 'w').write(
        '\n'.join('%.4f %.4f %d %d' % x for x in r) + '\n')
    fw, bw = r[:steps], r[steps:]
    for tag, rr in (('fwd', fw), ('bwd', bw)):
        ch = sorted(x[1] for x in rr)
        med = ch[len(ch) // 2]
        worst = sorted(rr, key=lambda x: -x[1])[:6]
        print('%s %s %s: median change %.4f | worst %s' % (pair, STYLES[st], tag, med,
              ' '.join('t%.4f:%.3f/max%d/big%d' % x for x in worst)))
    return r


if __name__ == '__main__':
    c, a = sys.argv[1], sys.argv[2:]
    if c == 'scrub':
        scrub(a[0], a[1], a[2] if len(a) > 2 else 'ram_crown', a[3:])


def strip(out, style, pair='ram_crown', extra=(), bundle='LoadEhrWow', n=11, tag=None):
    """an n-step strip t 0 .. 1 -> <out>/<tag>.png"""
    import subprocess
    os.makedirs(out, exist_ok=True)
    a, b = pair.split('_')
    st = STYLES.index(style) if not str(style).isdigit() else int(style)
    tag = tag or 'strip_%s_%s' % (pair, STYLES[st].replace(' ', ''))
    L = pair_setup(bundle, a, b) + ['set Transition_Style %d' % st] + list(extra) + ['set Transition 0.5', 'realtime 0.8']
    ts = [k / (n - 1) for k in range(n)]
    for i, t in enumerate(ts):
        L += ['set Transition %g' % t, 'advance 0 zero', 'frames 2', 'shot %s/%s_%02d.png' % (out, tag, i)]
    W.harness(L, out, tag)
    subprocess.run(['montage'] + ['%s/%s_%02d.png' % (out, tag, i) for i in range(n)] + ['-tile', '%dx1' % n, '-geometry', '256x144+1+1',
                   '%s/%s.png' % (out, tag)], capture_output=True)
    for i in range(n):
        os.remove('%s/%s_%02d.png' % (out, tag, i))
    return '%s/%s.png' % (out, tag)


def t_of_e(e, ease=1.0):
    """the slider t whose eased progress is e (e = t + ease (smoothstep(t) - t))"""
    lo, hi = 0.0, 1.0
    for _ in range(50):
        m = (lo + hi) / 2
        if m + ease * (m * m * (3 - 2 * m) - m) < e:
            lo = m
        else:
            hi = m
    return lo


def spike(path, elo=0.4, ehi=0.6, steps=400):
    """the swap window's worst step (the window in eased progress e, Ease 1) vs the hop's own motion (the worst step outside it),
    forward half"""
    lo, hi = t_of_e(elo), t_of_e(ehi)
    r = [tuple(map(float, l.split())) for l in open(path)][:steps]
    inn = max(x[1] for x in r if lo <= x[0] <= hi)
    outs = max(x[1] for x in r if not lo <= x[0] <= hi)
    return inn, outs


def realtime(out, style, pair, root=ROOT, tag='', frames=900):
    """Arena's W6 clip dials (Spin Y 3, Key 0.8, Ping-Pong, 6 s a hop, Hold 1.5 s) played by Transition Play at a fixed 60 Hz: the
    per-frame mean change over one cycle (15 s); prints the median, p99, max and where the max falls"""
    os.makedirs(out, exist_ok=True)
    a, b = pair.split('_')
    st = STYLES.index(style)
    L = ['bundle %s/build/LoadEhrWow.bundle' % root, 'size 640 360', 'instance'] + slot_a(a) + ['mark', 'text Model_B ' + M[b], 'wait_slots 1 60',
         'set Key Intensity 0.8', 'set Spin Y 3', 'set Transition_Style %d' % st, 'set Transition 0.5', 'realtime 1.0', 'set Transition 0',
         'set Transition Time 6', 'set Hold Time 1.5', 'set Transition Play 2', 'advance 0 zero', 'diffs %d' % frames]
    so, _ = W.harness(L, out, 'rt_%s_%s%s' % (pair, style, tag))
    d = [float(x) for x in re.search(r'diffs:(.*)', so).group(1).split()]
    s = sorted(d)
    i = max(range(len(d)), key=lambda k: d[k])
    print('%s %s %s: frames %d median %.3f p99 %.3f max %.3f at frame %d (%.2f s)' % (pair, style, tag, len(d), s[len(s) // 2], s[int(len(s) * 0.99)], d[i], i, i / 60))
    open(os.path.join(out, 'rt_%s_%s%s.txt' % (pair, style, tag)), 'w').write('\n'.join('%.4f' % x for x in d) + '\n')
    return d


SWAPW = {'Twist': (0.32, 0.68), 'Melt': (0.4, 0.6)}


def spikes(out, styles=('Twist', 'Melt'), pairs=(('skull_helmet', 'LoadEhr'), ('text_skull', 'LoadEhr'), ('ram_crown', 'LoadEhrWow'))):
    """each style x pair scrubbed both ways: the swap window's worst step vs the hop's own motion (forward and backward)"""
    rows = ['| pair | style | fwd swap / own | bwd swap / own | fwd = bwd (step for step) |', '|---|---|---|---|---|']
    for p, bundle in pairs:
        for st in styles:
            r = scrub(out, st, p, (), bundle)
            path = os.path.join(out, 'scrub_%s_%s.txt' % (p, st))
            lo, hi = t_of_e(SWAPW[st][0]), t_of_e(SWAPW[st][1])
            fw, bw = r[:400], r[400:][::-1]
            res = []
            for rr in (fw, bw):
                inn = max(x[1] for x in rr if lo <= x[0] <= hi)
                own = max(x[1] for x in rr if not lo <= x[0] <= hi)
                res.append('%.3f / %.3f (%.2f)' % (inn, own, inn / own))
            same = all(abs(a[1] - b[1]) < 1e-3 for a, b in zip(fw, bw))
            rows.append('| %s | %s | %s | %s | %s |' % (p, st, res[0], res[1], same))
            print(rows[-1], flush=True)
    open(os.path.join(out, 'spikes.md'), 'w').write('\n'.join(rows) + '\n')


def cost(out, root=ROOT, tag=''):
    """harness 1080p plugin / GPU ms mid-hop, skull -> helmet (Wow): the changed styles and Trans Sparkle"""
    os.makedirs(out, exist_ok=True)
    L = ['bundle %s/build/LoadEhrWow.bundle' % root, 'size 1920 1080', 'instance'] + slot_a('skull') + ['mark', 'text Model_B ' + M['helmet'],
         'wait_slots 1 60'] + CLIP + ['realtime 1.0']
    cases = [('rest (Transition 0)', ['set Transition 0', 'set Transition_Style 0', 'set Trans Sparkle 0'])]
    for st in ('Dissolve', 'Twist', 'Melt', 'Lava', 'Points', 'Spin Swap'):
        cases.append(('%s t 0.5' % st, ['set Transition_Style %d' % STYLES.index(st), 'set Trans Sparkle 0', 'set Transition 0.5']))
    for st in ('Dissolve', 'Twist', 'Points', 'Spin Swap'):
        for sp in (2, 10):
            cases.append(('%s t 0.5 Sparkle %d' % (st, sp), ['set Transition_Style %d' % STYLES.index(st), 'set Trans Sparkle %d' % sp, 'set Transition 0.5']))
    cases.append(('Dissolve t 0.5 Sparkle 0 (rest of the sparkle code: off)', ['set Transition_Style 0', 'set Trans Sparkle 0', 'set Transition 0.5']))
    for i, (c, cmd) in enumerate(cases):
        L += cmd + ['realtime 1.0', 'stats_reset', 'frames 120', 'stats C%d' % i]
    so, _ = W.harness(L, out, 'cost' + tag)
    rows = ['| case | plugin ms avg | GPU ms avg | GPU ms p99 |', '|---|---|---|---|']
    for i, (c, _) in enumerate(cases):
        m = re.search(r'stats C%d\s.*?plugin wall ms avg\s+([\d.]+).*?GPU ms avg\s+([\d.]+)\s+p99\s+([\d.]+)' % i, so)
        rows.append('| %s | %s | %s | %s |' % ((c,) + (m.groups() if m else ('?', '?', '?'))))
    print('\n'.join(rows))
    open(os.path.join(out, 'cost%s.md' % tag), 'w').write('\n'.join(rows) + '\n')
