#!/usr/bin/env python3
"""LoadEhr W9 Stage B: the Build looks (Slit-Scan, Print On, Exploded, Black Hole) in the harness.

usage: tools/w9_build.py renders <out>   every Build Look on the skull, the SciFi helmet, text "LOADEHR" and our SVG emblem at the default
                                        (Amount 0.5) + the "too much" end (Amount 1), tilted so the print's cap shows -> renders.md
       tools/w9_build.py strips <out>    progress strips 0 .. 1 .. 0 (Print On, Exploded, Black Hole: forward = backward, max diff), the
                                        slit-scan on a spinning skull, Build Speed / Build On Beat running a look -> strips.md
       tools/w9_build.py combos <out>    Print On + Wire Only (the unprinted part as faint wire), Black Hole over the FX input (the lens
                                        bends the background), Exploded text + Letter Wave, Slit-Scan + Twist, Print On mid-hop -> combos.md
       tools/w9_build.py cost <out>      harness 1080p plugin / GPU ms per look (default / top) -> cost.md"""
import os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import w5_wire as W  # noqa: E402

LOOKS = [(1, 'slit'), (2, 'print'), (3, 'exploded'), (4, 'hole')]
INPUT = os.path.join(W.ROOT, 'test_images', 'projector_test.png')


def build(n, amt=0.5, extra=()):
    return ['set Build Look %d' % n, 'set Build Amount %g' % amt] + ['set %s %g' % kv for kv in extra]


def shot(out, tag, settle='realtime 1.2'):
    return [settle, 'composite arena', 'shot %s/%s.png' % (out, tag), 'shot_rgba %s/%s_raw.png' % (out, tag)]


def cov(out, tag):
    c = W.coverage('%s/%s_raw.png' % (out, tag))
    os.remove('%s/%s_raw.png' % (out, tag))
    return c


def renders(out):
    os.makedirs(out, exist_ok=True)
    rows = ['| model | look | Amount | coverage | shot |', '|---|---|---|---|---|']
    for name, m in W.MODELS:
        L = W.load(m, bundle='LoadEhr') + ['set Rotate X 0', 'set Orbit Pitch 30', 'realtime 1']
        tags = []
        for n, ln in LOOKS:
            for amt in (0.5, 1.0):
                tag = '%s_%s_%s' % (name, ln, 'def' if amt < 1 else 'top')
                spin = ['set Spin Y 60'] if n == 1 else ['set Spin Y 0', 'pulse Reset Spin']
                L += spin + build(n, amt) + shot(out, tag, 'realtime 2.5' if n == 1 else 'realtime 1.2')
                tags.append((ln, amt, tag))
        W.harness(L + ['set Build Look 0'], out, 'r_' + name)
        for ln, amt, tag in tags:
            rows.append('| %s | %s | %g | %.4f | %s.png |' % (name, ln, amt, cov(out, tag), tag))
        print('\n'.join(rows[-len(tags):]), flush=True)
    open(os.path.join(out, 'renders.md'), 'w').write('\n'.join(rows) + '\n')


def strips(out):
    os.makedirs(out, exist_ok=True)
    rows = ['| strip | coverage per step | forward = backward (max of the compares) |', '|---|---|---|']
    steps = [0, 0.25, 0.5, 0.75, 1.0]
    for n, ln in LOOKS[1:]:
        L = W.load(W.SKULL, bundle='LoadEhr') + ['set Rotate X 0', 'set Orbit Pitch 30', 'realtime 1', 'set Build Look %d' % n]
        tags = []
        for i, a in enumerate(steps + steps[-2::-1]):
            tag = '%s_%02d' % (ln, i)
            L += ['set Build Amount %g' % a] + shot(out, tag, 'frames 4')
            tags.append(tag)
        k = len(steps)
        L += ['compare %s/%s.png %s/%s.png' % (out, tags[i], out, tags[2 * k - 2 - i]) for i in range(k - 1)]
        so, _ = W.harness(L, out, 's_' + ln)
        m = [int(x) for x in re.findall(r'max (\d+) pixels', so)]
        rows.append('| %s, Build Amount 0 .. 1 .. 0 | %s | %s |' % (ln, ' '.join('%.3f' % cov(out, t) for t in tags), max(m) if m else '?'))
        print(rows[-1], flush=True)
    # slit-scan: a spinning skull, the delay (Amount 0 / 0.5 / 1) and Build Speed waves
    L = W.load(W.SKULL, bundle='LoadEhr') + ['set Spin Y 60', 'set Build Look 1']
    tags = []
    for tag, cmd in (('slit_a0', ['set Build Amount 0']), ('slit_a05', ['set Build Amount 0.5']), ('slit_a1', ['set Build Amount 1']),
                     ('slit_waves', ['set Build Amount 0.6', 'set Build Speed 20'])):
        L += cmd + shot(out, tag, 'realtime 2.5')
        tags.append(tag)
    W.harness(L, out, 's_slit')
    rows.append('| slit-scan, Spin Y 60: Amount 0 / 0.5 / 1, waves (Speed 20) | %s | |' % ' '.join('%s %.3f' % (t, cov(out, t)) for t in tags))
    # Build Speed / On Beat run a look by themselves (mean per-frame change, the model still)
    res = []
    for tag, cmd in (('print_speed0', build(2)), ('print_speed10', build(2, 1, (('Build Speed', 10),))),
                     ('exploded_beat', build(3, 1, (('Build On Beat', 1),))), ('hole_speed10', build(4, 0.8, (('Build Speed', 10),)))):
        so, _ = W.harness(W.load(W.SKULL, bundle='LoadEhr') + cmd + ['realtime 1', 'diffs 60'], out, 'd_' + tag)
        d = [list(map(float, l.split()[1:])) for l in so.splitlines() if l.startswith('diffs:')]
        res.append('%s %.4f' % (tag, sum(d[0]) / len(d[0])) if d else tag + ' ?')
    rows.append('| mean per-frame change, model still | %s | |' % ', '.join(res))
    print('\n'.join(rows[-2:]))
    open(os.path.join(out, 'strips.md'), 'w').write('\n'.join(rows) + '\n')


def combos(out):
    os.makedirs(out, exist_ok=True)
    rows = ['| combination | coverage | shot |', '|---|---|---|']
    hole_fx = ['bundle %s/build/LoadEhrFX.bundle' % W.ROOT, 'size 640 360', 'input ' + INPUT, 'instance', 'text Model_A ' + W.SKULL, 'wait_loaded 60',
               'set Input Use 1']
    for tag, L in (('print_wireonly', W.load(W.HELMET, bundle='LoadEhr') + ['set Orbit Pitch 30'] + build(2, 0.55) + ['set Wire Mode 2', 'set Wireframe Mix 0.35']),
                   ('hole_fx_input', hole_fx + build(4, 0.6)),
                   ('exploded_text_wave', W.load('text', bundle='LoadEhr') + build(3, 0.6) + ['set Letter Wave 0.8']),
                   ('slit_twist', W.load(W.SKULL, bundle='LoadEhr') + build(1, 0.8) + ['set Spin Y 45', 'set Twist 2']),
                   ('print_midhop', W.load(W.SKULL, bundle='LoadEhr') + ['mark', 'text Model_B ' + W.HELMET, 'wait_slots 1 60', 'set Orbit Pitch 30'] +
                    build(2, 0.6) + ['set Transition 0.5'])):
        W.harness(L + shot(out, tag, 'realtime 2.5'), out, 'c_' + tag)
        rows.append('| %s | %.4f | %s.png |' % (tag, cov(out, tag), tag))
        print(rows[-1], flush=True)
    open(os.path.join(out, 'combos.md'), 'w').write('\n'.join(rows) + '\n')


def cost(out):
    os.makedirs(out, exist_ok=True)
    cases = [('solid (Build Look Off)', ['set Build Look 0'])]
    for n, ln in LOOKS:
        cases += [('%s default (0.5)' % ln, build(n, 0.5)), ('%s top (1)' % ln, build(n, 1.0))]
    rows = ['| case | skull plugin ms avg / GPU ms avg | helmet |', '|---|---|---|']
    res = {}
    for name, m in W.MODELS[:2]:
        L = W.load(m, (1920, 1080), bundle='LoadEhr') + ['set Spin Y 20', 'set Orbit Pitch 30', 'realtime 1.5']
        for i, (c, w) in enumerate(cases):
            L += w + ['realtime 1.5', 'stats_reset', 'frames 120', 'stats C%d' % i]
        so, _ = W.harness(L, out, 'cost_' + name)
        for i in range(len(cases)):
            m2 = re.search(r'stats C%d\s.*?plugin wall ms avg\s+([\d.]+).*?GPU ms avg\s+([\d.]+)' % i, so)
            res[(name, i)] = '%s / %s' % m2.groups() if m2 else '?'
    for i, (c, _) in enumerate(cases):
        rows.append('| %s | %s | %s |' % (c, res[('skull', i)], res[('helmet', i)]))
    print('\n'.join(rows))
    open(os.path.join(out, 'cost.md'), 'w').write('\n'.join(rows) + '\n')


if __name__ == '__main__':
    {'renders': renders, 'strips': strips, 'combos': combos, 'cost': cost}[sys.argv[1]](sys.argv[2])
