#!/usr/bin/env python3
"""W27: the mirror room, finished. Every volume and particle reflects. The looks here = the W27 Arena clips (CLIPS) + one
test look per feature, cycled over the three room shapes (TESTS). Renders reuse tools/w26_wow.py (Wow bundles, the arena composite,
clocks zeroed, host time pinned).
usage: w27_mirror.py run <tag> [name ...]      t0.. + strip.png per look -> verify/w27/<tag>/<slug>/
       w27_mirror.py before <tag> [name ...]   the same with HARNESS=<the W26 harness> (env W27_BEFORE = its build dir)
       w27_mirror.py pair <after> <before>     before (top) / after (bottom) strips -> verify/w27/<after>/<slug>/pair.png
       w27_mirror.py perf [name ...]           GPU ms per look at 1080p (sanity), Mirror Count 3 and 8, + the room off
       w27_mirror.py arena set|check <layer> <first col>   the 5 CLIPS into Arena (Wow layer), by param name, untriggered
       w27_mirror.py list"""
import os, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import w26_wow as V
import w22_smoke as W

ROOT, SKULL, MASK, colour = V.ROOT, V.SKULL, V.MASK, W.colour
IMG = ROOT + '/test_images/projector_test.png'


def room(shape, count=3, size=1.5, depth=1.5, reflect=0.8):
    return [('Mirror Room', shape), ('Mirror Count', count), ('Box Size', size), ('Box Depth', depth), ('Mirror Color', 0.58),
            ('Mirror Sat', 0.15), ('Mirror Bri', 1.0), ('Mirror Reflect', reflect), ('Mirror Gap', 0.8), ('LED Colors', 1), ('LED Speed', 0.2)]


def look(name, e):
    return next(x for x in V.LOOKS if x['name'] == name)['dials'] if e is None else e


# the five W27 clips (Arena layer "Mirror"): soft motion, the room on, everything else a W22 / W26 recipe
CLIPS = [
    dict(name='Mirror Smoke', model=SKULL, base=1, dials=room(1) + [('Distance', 7), ('Orbit Yaw', 25), ('Orbit Pitch', 12), ('Spin Y', 5),
                                                                      ('Smoke Ambient', 0.5)]),
    dict(name='Mirror Fire', model=SKULL, dials=look('Eyes Of Fire', None) + room(2, 4, 1.6, 1.6) + [('Orbit Yaw', 20), ('Orbit Pitch', 6)]),
    dict(name='Mirror Swarm', model=SKULL, bpm=120, times=(1, 2.2, 3.4, 5), dials=look('Firefly Face', None) + room(1) +
         [('Distance', 7), ('Orbit Yaw', 25), ('Orbit Pitch', 12)]),
    dict(name='Mirror Storm', model=SKULL, dials=look('Tesla Skull', None) + room(3, 6, 1.5, 1.5) +
         [('Haze', 0.2), ('Smoke Amount', 1.0),  # LoadEhr: MaskEhr's two eye beams removed
          ('Orbit Yaw', 15), ('Orbit Pitch', 8), ('Spin Y', 4)]),
    dict(name='Mirror Projector', model=SKULL, dials=room(2, 4, 1.6, 1.6) +
         [('Proj Image', IMG), ('Proj Enable', 1), ('Proj Pos X', 0.9), ('Proj Pos Y', 1.0), ('Proj Pos Z', 1.3), ('Proj Throw', 30),
          ('Proj Intensity', 40), ('Proj Surface', 1.2), ('Proj Beam', 1.0), ('Haze', 0.3), ('Smoke Amount', 1.0), ('Smoke Speed', 0.25),
          ('Key Intensity', 0.8), ('Env Intensity', 0.4), ('Orbit Yaw', 20), ('Orbit Pitch', 6), ('Spin Y', 4)]),
]

LIGHT_A = [('A Enable', 1), ('A Intensity', 20), ('A Cone Angle', 40), ('Haze', 0.25), ('Smoke Amount', 1.2), ('Glow Cloud', 1.5)]
# one look per feature, the shapes cycled (Box, Corridor, Two Mirrors)
TESTS = [
    dict(name='T Sim Smoke', model=SKULL, base=1, dials=room(1) + [('Distance', 7), ('Orbit Yaw', 25), ('Orbit Pitch', 12)]),
    dict(name='T Fire', model=SKULL, dials=look('Eyes Of Fire', None) + room(2)),
    dict(name='T Shaft', model=SKULL, dials=LIGHT_A + room(3) + [('Orbit Yaw', 20)]),
    dict(name='T Projector', model=SKULL, dials=look('Projection Mapped', None) + [('Proj Pos X', 0.9), ('Proj Pos Y', 0.8), ('Proj Pos Z', 1.3)] + room(1)),
    dict(name='T Glow Motes', model=SKULL, dials=LIGHT_A + [('Motes', 3)] + room(2)),
    dict(name='T Swarm', model=MASK, dials=look('Firefly Halo', None) + room(3, 5)),
    dict(name='T Lightning', model=SKULL, dials=look('Tesla Skull', None) + room(1) + [('Haze', 0.15), ('Smoke Amount', 1)]),
    dict(name='T Shards', model=SKULL, times=(0.4, 1.0, 2.0, 3.0), start=['pulse Shatter'], dials=look('Bounce Shards', None) + room(2) +
         [('Shadow Catcher', 0), ('Mirror Floor', 0)]),
    dict(name='T Corridor Box', model=MASK, dials=look('Infinity Corridor', None) + [('Mirror Room', 1), ('Haze', 0.2)]),  # MaskEhr's T Lasers Box, beams removed
]
ALL = CLIPS + TESTS


def slug(e):
    return e['name'].lower().replace(' ', '_')


def find(names):
    return [e for e in ALL if not names or e['name'] in names or slug(e) in names]


def render(tag, names, harness=None):
    out = os.path.join(ROOT, 'verify', 'w27', tag)
    if harness:
        V.HARNESS = harness
    for e in find(names):
        d = os.path.join(out, slug(e))
        os.makedirs(d, exist_ok=True)
        L, t0 = V.preamble(e), 0
        if harness:  # the W26 bundles next to that harness
            L = [l.replace(ROOT + '/build/', os.path.dirname(harness) + '/') for l in L]
        times = e.get('times', (1, 2, 3, 5))
        for i, s in enumerate(times):
            L += ['frames %d' % round((s - t0) * 60), 'shot %s/t%d.png' % (d, i)]
            t0 = s
        log = V.harness(L, os.path.join(d, 'script.txt'))
        open(os.path.join(d, 'run.log'), 'w').write(log)
        V.montage([os.path.join(d, 't%d.png' % i) for i in range(len(times))], ['%g s' % s for s in times], '%s  %s' % (tag, e['name']),
                  os.path.join(d, 'strip.png'), '%dx1' % len(times))
        print(d)


def pair(after, before):
    for e in ALL:
        a = os.path.join(ROOT, 'verify', 'w27', after, slug(e), 'strip.png')
        b = os.path.join(ROOT, 'verify', 'w27', before, slug(e), 'strip.png')
        if os.path.exists(a) and os.path.exists(b):
            subprocess.run(['magick', b, a, '-append', os.path.join(os.path.dirname(a), 'pair.png')], check=True)
            print(os.path.join(os.path.dirname(a), 'pair.png'))


def perf(names):
    for e in find(names):
        for label, extra in (('count 3', [('Mirror Count', 3)]), ('count 8', [('Mirror Count', 8)]), ('room off', [('Mirror Room', 0)])):
            x = dict(e, dials=list(e['dials']) + extra)
            L = V.preamble(x) + ['frames 120', 'stats_reset', 'frames 240', 'stats %s | %s' % (e['name'], label)]
            out = V.harness(L, '/tmp/w27_perf.txt')
            print('\n'.join(l for l in out.splitlines() if l.startswith('stats')))


if __name__ == '__main__':
    a = sys.argv[1:]
    if not a or a[0] == 'list':
        print('\n'.join(e['name'] for e in ALL))
    elif a[0] == 'run':
        render(a[1], a[2:])
    elif a[0] == 'before':
        render(a[1], a[2:], os.path.join(os.environ.get('W27_BEFORE', '/tmp/loadehr_w26/build'), 'loadehr_harness'))
    elif a[0] == 'pair':
        pair(a[1], a[2])
    elif a[0] == 'perf':
        perf(a[1:])
    elif a[0] == 'arena':
        import w26_arena as R
        for i, e in enumerate(CLIPS):
            (R.set_look if a[1] == 'set' else R.check)(int(a[2]), int(a[3]) + i, e)
