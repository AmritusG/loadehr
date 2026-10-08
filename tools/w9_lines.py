#!/usr/bin/env python3
"""LoadEhr W9 Stage A: the line looks (Lines group, classic bundles) in the harness.

usage: tools/w9_lines.py renders <out>   every Line Look on the skull, the SciFi helmet, text "LOADEHR" and our SVG emblem at the
                                        defaults + the "too much" end (Amount 1, Reach 1, Speed 4) -> <out>/*.png + renders.md
       tools/w9_lines.py strips <out>    what moves: light paint on a spinning skull (5 shots, 0.4 s apart), plexus through a shatter
                                        (5 flight times), plexus mid-hop skull -> helmet (Transition 0.2 .. 0.8), neon with Line Speed
                                        (flow), Line Audio (fft pulse vs silent) -> strips.md
       tools/w9_lines.py combos <out>    Plexus + Points Only, Light Paint + Twist, Neon Tubes + Wire Glow 3 + Bloom 3 -> combos.md
       tools/w9_lines.py cost <out>      harness 1080p plugin / GPU ms: solid, plexus 5k / 20k points, light paint default / top, neon
                                        default / top -> cost.md"""
import os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import w5_wire as W  # noqa: E402

LOOKS = [(1, 'plexus'), (2, 'paint'), (3, 'neon')]
TOP = [('Line Amount', 1), ('Line Reach', 1), ('Line Speed', 4)]


def look(n, extra=()):
    return ['set Line Look %d' % n] + ['set %s %g' % kv for kv in extra]


def shot(out, tag, settle='realtime 1.2'):
    return [settle, 'composite arena', 'shot %s/%s.png' % (out, tag), 'shot_rgba %s/%s_raw.png' % (out, tag)]


def cov(out, tag):
    c = W.coverage('%s/%s_raw.png' % (out, tag))
    os.remove('%s/%s_raw.png' % (out, tag))
    return c


def lines_log(log):
    return re.findall(r'LINES (plexus|light paint|neon tubes) on .*?: (\d+) spots, (\d+) pairs', log)


def renders(out):
    os.makedirs(out, exist_ok=True)
    rows = ['| model | look | case | spots / pairs (worker) | coverage | shot |', '|---|---|---|---|---|---|']
    for name, m in W.MODELS:
        L = W.load(m, bundle='LoadEhr') + ['set Spin Y 20', 'realtime 1']
        tags = []
        for n, ln in LOOKS:
            for case, extra in (('default', ()), ('too much', TOP)):
                tag = '%s_%s_%s' % (name, ln, 'top' if extra else 'def')
                L += ['set Line Amount 0.35', 'set Line Reach 0.3', 'set Line Speed 0.5'] + look(n, extra) + shot(out, tag, 'realtime 2')
                tags.append((ln, case, tag))
        so, log = W.harness(L, out, 'r_' + name)
        ll = lines_log(log)
        for i, (ln, case, tag) in enumerate(tags):
            sp = '%s / %s' % (ll[i][1], ll[i][2]) if i < len(ll) else '?'
            rows.append('| %s | %s | %s | %s | %.4f | %s.png |' % (name, ln, case, sp, cov(out, tag), tag))
        print('\n'.join(rows[-len(tags):]), flush=True)
    open(os.path.join(out, 'renders.md'), 'w').write('\n'.join(rows) + '\n')


def strips(out):
    os.makedirs(out, exist_ok=True)
    rows = ['| strip | shots |', '|---|---|']
    # light paint on a spinning skull: the trails grow, then keep their length
    L = W.load(W.SKULL, bundle='LoadEhr') + look(2) + ['set Spin Y 40', 'realtime 0.3']
    t = []
    for i in range(5):
        L += shot(out, 'paint_spin_%d' % i, 'realtime 0.4')
        t.append('paint_spin_%d' % i)
    W.harness(L, out, 's_paint')
    rows.append('| light paint, Spin Y 40, every 0.4 s | %s |' % ' '.join('%s (%.4f)' % (x, cov(out, x)) for x in t))
    # plexus through a shatter: pairs flying apart stretch and fade
    L = W.load(W.SKULL, bundle='LoadEhr') + look(1) + ['realtime 1', 'pulse Shatter']
    t = []
    for i in range(5):
        L += shot(out, 'plexus_shatter_%d' % i, 'realtime 0.25')
        t.append('plexus_shatter_%d' % i)
    W.harness(L, out, 's_shatter')
    rows.append('| plexus, Shatter, every 0.25 s | %s |' % ' '.join('%s (%.4f)' % (x, cov(out, x)) for x in t))
    # plexus mid-hop: the outgoing model's lines fade out, the incoming one's in
    L = W.load(W.SKULL, bundle='LoadEhr') + ['mark', 'text Model_B ' + W.HELMET, 'wait_slots 1 60'] + look(1) + ['set Transition Style 12']
    t = []
    for i, tr in enumerate((0.2, 0.4, 0.5, 0.6, 0.8)):
        L += ['set Transition %g' % tr] + shot(out, 'plexus_hop_%d' % i, 'realtime 1')
        t.append('plexus_hop_%d' % i)
    W.harness(L, out, 's_hop')
    rows.append('| plexus mid-hop skull -> helmet (Twist), Transition 0.2 / 0.4 / 0.5 / 0.6 / 0.8 | %s |' % ' '.join('%s (%.4f)' % (x, cov(out, x)) for x in t))
    # Line Speed / Line Audio move the picture with the model still (mean per-frame change)
    res = []
    for tag, cmd in (('neon_speed0', look(3, (('Line Speed', 0),))), ('neon_speed2', look(3, (('Line Speed', 2),))),
                     ('plexus_audio0', look(1, (('Line Speed', 0), ('Line Audio', 0)))), ('plexus_audio3', look(1, (('Line Speed', 0), ('Line Audio', 3))))):
        L = W.load(W.SKULL, bundle='LoadEhr') + ['fft 0.8'] + cmd + ['realtime 1.2', 'diffs 30']
        so, _ = W.harness(L, out, 's_' + tag)
        d = [list(map(float, l.split()[1:])) for l in so.splitlines() if l.startswith('diffs:')]
        res.append('%s %.4f' % (tag, sum(d[0]) / len(d[0])) if d else tag + ' ?')
    rows.append('| model still, mean per-frame change | %s |' % ', '.join(res))
    print('\n'.join(rows))
    open(os.path.join(out, 'strips.md'), 'w').write('\n'.join(rows) + '\n')


def combos(out):
    os.makedirs(out, exist_ok=True)
    rows = ['| combination | coverage | shot |', '|---|---|---|']
    for tag, m, cmd in (('plexus_points', W.SKULL, look(1) + ['set Wire Mode 4', 'set Point Source 2', 'set Wire Color Mix 1']),
                        ('paint_twist', 'text', look(2, (('Line Amount', 0.6),)) + ['set Twist 3', 'set Spin Y 30']),
                        ('neon_glow_bloom', 'logo', look(3) + ['set Wire Glow 3', 'set Wire Bloom 3'])):
        W.harness(W.load(m, bundle='LoadEhr') + cmd + shot(out, tag, 'realtime 2'), out, 'c_' + tag)
        rows.append('| %s | %.4f | %s.png |' % (tag, cov(out, tag), tag))
        print(rows[-1], flush=True)
    open(os.path.join(out, 'combos.md'), 'w').write('\n'.join(rows) + '\n')


def cost(out):
    os.makedirs(out, exist_ok=True)
    a5 = ((5000 - 200) / 20000) ** 0.5
    cases = [('solid (Line Look Off)', look(0)), ('plexus 5k points (Amount %.2f)' % a5, look(1, (('Line Amount', a5),))),
             ('plexus 20k points (Amount 1)', look(1, (('Line Amount', 1),))), ('plexus 20k, Reach 1', look(1, (('Line Amount', 1), ('Line Reach', 1)))),
             ('light paint default (~390 heads, 1.3 s)', look(2)), ('light paint top (3020 heads, 4 s)', look(2, (('Line Amount', 1), ('Line Reach', 1)))),
             ('neon default', look(3)), ('neon top (Amount 1, Reach 1)', look(3, (('Line Amount', 1), ('Line Reach', 1))))]
    rows = ['| case | skull plugin ms avg / GPU ms avg | helmet |', '|---|---|---|']
    res = {}
    for name, m in W.MODELS[:2]:
        L = W.load(m, (1920, 1080), bundle='LoadEhr') + ['set Spin Y 20', 'realtime 1.5']
        for i, (c, w) in enumerate(cases):
            L += ['set Line Amount 0.35', 'set Line Reach 0.3'] + w + ['realtime 2', 'stats_reset', 'frames 120', 'stats C%d' % i]
        so, log = W.harness(L, out, 'cost_' + name)
        for i in range(len(cases)):
            m2 = re.search(r'stats C%d\s.*?plugin wall ms avg\s+([\d.]+).*?GPU ms avg\s+([\d.]+)' % i, so)
            res[(name, i)] = '%s / %s' % m2.groups() if m2 else '?'
        res[(name, 'log')] = lines_log(log)
    for i, (c, _) in enumerate(cases):
        rows.append('| %s | %s | %s |' % (c, res[('skull', i)], res[('helmet', i)]))
    rows += ['', 'worker lines (skull): ' + '; '.join('%s %s spots %s pairs' % x for x in res[('skull', 'log')])]
    print('\n'.join(rows))
    open(os.path.join(out, 'cost.md'), 'w').write('\n'.join(rows) + '\n')


if __name__ == '__main__':
    {'renders': renders, 'strips': strips, 'combos': combos, 'cost': cost}[sys.argv[1]](sys.argv[2])
