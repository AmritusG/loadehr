#!/usr/bin/env python3
"""LoadEhr W9 Stage D: the Surface Looks (Audio Spikes, Skin Of Copies, X-Ray, Thermal, Night Vision, LED Matrix; classic bundles).

usage: tools/w9_surface.py renders <out>   every Surf Look on the skull (B = the SciFi helmet), the helmet (B = skull), text "LOADEHR" and our
                                          SVG emblem (B = skull), default + the "too much" end (Amount 2, Scale 1, Speed 4), fft 0.6 -> renders.md
       tools/w9_surface.py strips <out>    spikes silent vs pulsing (fft 0 / 0.8), copies Amount 0 / 0.5 / 1 / 1.5 / 2, the camera looks
                                          Amount 0 / 0.5 / 1, Surf Speed moving them (mean per-frame change) -> strips.md
       tools/w9_surface.py combos <out>    Spikes + Twist + Spin, copies of the Text (Surf Source Text) on the skull, X-Ray + Print On, LED Matrix
                                          + Plexus, Thermal + Exploded, Night Vision + Wire Over Surface -> combos.md
       tools/w9_surface.py cost <out>      harness 1080p plugin / GPU ms per look (default / top) -> cost.md"""
import os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import w5_wire as W  # noqa: E402

LOOKS = [(1, 'spikes'), (2, 'copies'), (3, 'xray'), (4, 'thermal'), (5, 'nightvision'), (6, 'led')]
TOP = (('Surf Amount', 2), ('Surf Scale', 1), ('Surf Speed', 4))
DEF = ['set Surf Amount 0.6', 'set Surf Scale 0.35', 'set Surf Speed 0.5']


def surf(n, extra=()):
    return ['set Surf Look %d' % n] + ['set %s %g' % kv for kv in extra]


def shot(out, tag, settle='realtime 1.2'):
    return [settle, 'composite arena', 'shot %s/%s.png' % (out, tag), 'shot_rgba %s/%s_raw.png' % (out, tag)]


def cov(out, tag):
    c = W.coverage('%s/%s_raw.png' % (out, tag))
    os.remove('%s/%s_raw.png' % (out, tag))
    return c


def with_b(model, b):
    return W.load(model, bundle='LoadEhr') + ['mark', 'text Model_B ' + b, 'wait_slots 1 60']


def renders(out):
    os.makedirs(out, exist_ok=True)
    rows = ['| model | look | case | coverage | shot |', '|---|---|---|---|---|']
    for name, m, b in (('skull', W.SKULL, W.HELMET), ('helmet', W.HELMET, W.SKULL), ('text', 'text', W.SKULL), ('logo', 'logo', W.SKULL)):
        L = with_b(m, b) + ['fft 0.6', 'realtime 1']
        tags = []
        for n, ln in LOOKS:
            for case, extra in (('default', ()), ('too much', TOP)):
                tag = '%s_%s_%s' % (name, ln, 'top' if extra else 'def')
                L += DEF + surf(n, extra) + shot(out, tag, 'realtime 1.5')
                tags.append((ln, case, tag))
        W.harness(L, out, 'r_' + name)
        for ln, case, tag in tags:
            rows.append('| %s | %s | %s | %.4f | %s.png |' % (name, ln, case, cov(out, tag), tag))
        print('\n'.join(rows[-len(tags):]), flush=True)
    open(os.path.join(out, 'renders.md'), 'w').write('\n'.join(rows) + '\n')


def strips(out):
    os.makedirs(out, exist_ok=True)
    rows = ['| strip | coverage per step |', '|---|---|']
    for ln, n, steps in (('copies', 2, (0, 0.5, 1, 1.5, 2)), ('xray', 3, (0, 0.5, 1)), ('thermal', 4, (0, 0.5, 1)), ('nightvision', 5, (0, 0.5, 1)), ('led', 6, (0, 0.5, 1))):
        L = with_b(W.SKULL, W.HELMET) + surf(n, (('Surf Speed', 0),))
        tags = []
        for i, a in enumerate(steps):
            tag = '%s_amt_%d' % (ln, i)
            L += ['set Surf Amount %g' % a] + shot(out, tag, 'realtime 0.6')
            tags.append(tag)
        W.harness(L, out, 's_' + ln)
        rows.append('| %s, Surf Amount %s | %s |' % (ln, ' / '.join(map(str, steps)), ' '.join('%.3f' % cov(out, t) for t in tags)))
        print(rows[-1], flush=True)
    res = []
    for tag, cmd in (('spikes_silent', ['fft 0'] + surf(1, (('Surf Speed', 0),))), ('spikes_fft', ['fft 0.8'] + surf(1, (('Surf Speed', 0),))),
                     ('spikes_dance', ['fft 0'] + surf(1, (('Surf Speed', 2),))), ('copies_wobble', surf(2, (('Surf Speed', 2),))),
                     ('nightvision_grain', surf(5)), ('led_roll', surf(6, (('Surf Speed', 2),))), ('thermal_shimmer', surf(4, (('Surf Speed', 4),)))):
        so, _ = W.harness(with_b(W.SKULL, W.HELMET) + cmd + ['realtime 1.5', 'diffs 60'], out, 'd_' + tag)
        d = [list(map(float, l.split()[1:])) for l in so.splitlines() if l.startswith('diffs:')]
        res.append('%s %.4f' % (tag, sum(d[0]) / len(d[0])) if d else tag + ' ?')
    rows.append('| mean per-frame change, model still | %s |' % ', '.join(res))
    print(rows[-1])
    open(os.path.join(out, 'strips.md'), 'w').write('\n'.join(rows) + '\n')


def combos(out):
    os.makedirs(out, exist_ok=True)
    rows = ['| combination | coverage | shot |', '|---|---|---|']
    for tag, L in (('spikes_twist_spin', with_b(W.SKULL, W.HELMET) + ['fft 0.7', 'set Twist 1.5', 'set Spin Y 30'] + surf(1, (('Surf Amount', 1),))),
                   ('copies_of_text', with_b(W.SKULL, W.HELMET) + ['set Surf Source 3', 'text Text WOW'] + surf(2, (('Surf Scale', 0.55), ('Surf Amount', 1.2)))),
                   ('xray_print', with_b(W.HELMET, W.SKULL) + ['set Orbit Pitch 25', 'set Build Look 2'] + surf(3, (('Surf Amount', 1),))),
                   ('led_plexus', with_b(W.SKULL, W.HELMET) + ['set Line Look 1'] + surf(6, (('Surf Amount', 1), ('Surf Scale', 0.2)))),
                   ('thermal_exploded', with_b(W.HELMET, W.SKULL) + ['set Build Look 3', 'set Build Amount 0.6'] + surf(4, (('Surf Amount', 1),))),
                   ('nightvision_wire', with_b(W.SKULL, W.HELMET) + ['set Wire Mode 1', 'set Wireframe Mix 0.4'] + surf(5, (('Surf Amount', 1),)))):
        W.harness(L + shot(out, tag, 'realtime 2.5'), out, 'c_' + tag)
        rows.append('| %s | %.4f | %s.png |' % (tag, cov(out, tag), tag))
        print(rows[-1], flush=True)
    open(os.path.join(out, 'combos.md'), 'w').write('\n'.join(rows) + '\n')


def cost(out):
    os.makedirs(out, exist_ok=True)
    cases = [('solid (Surf Look Off)', ['set Surf Look 0'])]
    for n, ln in LOOKS:
        cases += [('%s default' % ln, DEF + surf(n)), ('%s top' % ln, surf(n, TOP))]
    rows = ['| case | skull plugin ms avg / GPU ms avg | helmet |', '|---|---|---|']
    res = {}
    for name, m, b in (('skull', W.SKULL, W.HELMET), ('helmet', W.HELMET, W.SKULL)):
        L = with_b(m, b) + ['fft 0.6', 'set Spin Y 20', 'realtime 1.5']
        L[1] = 'size 1920 1080'
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
