#!/usr/bin/env python3
"""W24: the Trip group. Each Trip look = a W22 smoke recipe (tools/w22_smoke.py EXAMPLES, the base) + Trip dials on top
(later dials win). The same list dials the Arena clips (tools/w24_arena.py) and docs/smoke_examples.md.
Every render is reproducible (W23): the SDF lands before the reset, clocks zeroed, host time pinned; over the textured
background (composite arena).
usage: w24_trip.py run [n ...]      5 s of each look -> verify/w24/trip/<nn>_<slug>/t{1,2,3,5}.png + strip.png
       w24_trip.py master [n ...]   the 3 s frame at Trip Master 0 / 0.5 / 1 -> verify/w24/trip/<nn>_<slug>/master.png
       w24_trip.py contact          verify/w24/trip/contact.png (the 3 s frame of each)
       w24_trip.py perf [n ...]     GPU ms per look at 1080p and of its base recipe (sanity only)
       w24_trip.py freeze           Freeze held for 1 s: the sim's smoke total must not change (SIMAIR lines)
       w24_trip.py md               print the Trip table (markdown)"""
import os, re, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import w22_smoke as W

ROOT = W.ROOT
OUT = os.path.join(ROOT, 'verify', 'w24', 'trip')
HARNESS = os.path.join(ROOT, 'build', 'loadehr_harness')
colour = W.colour

# white smoke and pale lamps, so the Trip colours are the colours (whole HSBA quartets: Arena sets a picker as one value)
WHITE = colour('Smoke', 0.0, 0.0, 1.0)
# the bases are tuned bright; coloured looks need the core out of the tonemap's white
FOG = [('Smoke Ambient', 0.25), ('A Intensity', 8)]
DRAGON = [('Smoke Amount', 2), ('A Intensity', 15)]

TRIPS = [
    dict(n=1, name='Rainbow Swirl', base=1, dials=WHITE + DRAGON + [
        *colour('A', 0.07, 0.1, 1.0), *colour('C Left', 0.07, 0.15, 1.0),
        ('Colour By', 4), ('Trip Palette', 0), ('Palette Shift', 0.1), ('Palette Spread', 1.5)]),
    dict(n=2, name='Oil Slick', base=3, dials=WHITE + FOG + [
        *colour('A', 0.58, 0.1, 1.0), ('A Intensity', 3), ('Smoke Ambient', 0.6), ('Thin Film', 1.2), ('Thin Film Thick', 1.2)]),
    dict(n=3, name='Prism', base=1, dials=WHITE + DRAGON + [
        *colour('A', 0.07, 0.1, 1.0), ('Prism', 1.5), ('Prism Angle', 0)]),
    dict(n=4, name='Mandala', base=3, dials=WHITE + FOG + [
        *colour('A', 0.58, 0.1, 1.0), ('Kaleido Smoke', 8), ('Kaleido Spin', 4),
        ('Colour By', 6), ('Trip Palette', 2), ('Palette Shift', 0.05)]),
    dict(n=5, name='Fractal Fog', base=3, dials=WHITE + FOG + [
        ('Fractal Fog', 1), ('Fractal Scale', 1), ('Fractal Iter', 3), ('Fractal Morph', 6)]),
    dict(n=6, name='Neon Veins', base=4, dials=[
        ('Neon Veins', 4), ('Vein Width', 0.5), *colour('Vein', 0.5, 1.0, 1.0)]),
    dict(n=7, name='Contours', base=3, dials=WHITE + FOG + [
        *colour('A', 0.58, 0.1, 1.0), ('A Intensity', 2), ('Smoke Ambient', 0), ('Smoke Contours', 4), ('Contour Bands', 5), ('Band Sharpness', 0.9), ('Band Scroll', 0.3),
        ('Trip Palette', 0)]),
    dict(n=8, name='Blacklight', base=1, dials=WHITE + [
        ('A Intensity', 6), ('C Intensity', 1), ('Smoke Ambient', 0),
        ('Blacklight', 3), ('Blacklight By', 1), *colour('Blacklight', 0.76, 0.85, 1.0)]),
    dict(n=9, name='Smoke Rings', base=1, dials=WHITE + DRAGON + [
        *colour('A', 0.07, 0.1, 1.0), ('Orbit Yaw', 70), ('Sim Res', 2), ('Smoke Fade', 1.5), ('Emit Eyes', 0), ('C Intensity', 1), ('Ring On Beat', 2), ('Ring From', 0), ('Ring Size', 0.6),
        ('Ring Speed', 3), ('Ring Smoke', 4)]),
    dict(n=10, name='Ghost Faces', base=1, dials=WHITE + DRAGON + [
        *colour('A', 0.07, 0.1, 1.0), ('Orbit Yaw', 0), ('Sim Res', 2), ('Emit Eyes', 0.5), ('Smoke Ghosts', 0.6), ('Ghost Offset', 1), ('Ghost Scale', 1.3),
        ('Ghost Fade Time', 1.5)]),
    dict(n=11, name='Hyperspace', base=3, dials=WHITE + FOG + [
        *colour('A', 0.58, 0.1, 1.0), ('Hyperspace', 2), ('Colour By', 3), ('Trip Palette', 0), ('Palette Shift', 0.1)]),
    dict(n=12, name='Hue Echo', base=6, dials=[
        ('Hue Echo', 0.9), ('Echo Hue Step', 0.02)]),
    dict(n=13, name='Time Warp', base=1, dials=[
        ('Time Warp', 0.4)]),
    dict(n=14, name='Bonanza', base=1, dials=WHITE + DRAGON + [
        *colour('A', 0.07, 0.1, 1.0), *colour('C Left', 0.07, 0.15, 1.0), ('Trip Master', 0.7),
        ('Colour By', 4), ('Trip Palette', 0), ('Palette Shift', 0.15), ('Palette Spread', 1.5),
        ('Kaleido Smoke', 6), ('Kaleido Spin', 3), ('Thin Film', 0.5), ('Neon Veins', 2), ('Hue Echo', 0.85),
        ('Prism', 0.5), ('Whirlpool', 1), ('Ring On Beat', 3)]),
]

OPTIONS = {'Colour By': 'Off|Density|Heat|Speed|Swirl|Age|Height', 'Trip Palette': 'Rainbow|Acid|Neon|Fire|Ice|Picker Pair',
           'Blacklight By': 'Density|Heat', 'Ring On Beat': 'Off|Every Beat|Every 2|Every 4|Every 8',
           'Ring From': 'Eyes|Mouth|Behind'}
W.OPTIONS.update(OPTIONS)


def recipe(t, extra=()):
    """the Trip look as a W22-style example: the base recipe's dials, then the Trip dials (later ones win)"""
    b = next(e for e in W.EXAMPLES if e['n'] == t['base'])
    e = dict(b)
    e['dials'] = list(b['dials']) + list(t['dials']) + list(extra)
    e['n'], e['name'] = t['n'], t['name']
    return e


def slug(t):
    return '%02d_%s' % (t['n'], t['name'].lower().replace(' ', '_'))


def preamble(e):
    L = W.script(e, OUT).splitlines()
    i = L.index('pulse Sim Reset')
    return L[:i] + ['realtime 1.5', 'advance 0 zero', 'hosttime 0', 'pulse Sim Reset', 'composite arena']


def harness(lines, path, env=None):
    open(path, 'w').write('\n'.join(lines) + '\n')
    r = subprocess.run([HARNESS, path], capture_output=True, text=True, cwd=ROOT, env=env)
    return r.stdout + r.stderr


def by_n(args):
    ns = [int(a) for a in args]
    return [t for t in TRIPS if not ns or t['n'] in ns]


def montage(frames, labels, title, dst, tile):
    cmd = ['montage'] + sum([['-label', l, f] for l, f in zip(labels, frames)], []) + [
        '-tile', tile, '-geometry', '640x360+4+4', '-background', '#202020', '-fill', 'white', '-font', W.FONT,
        '-pointsize', '20', '-title', title, dst]
    subprocess.run(cmd, check=True)


def run(args):
    W.ensure_input()
    for t in by_n(args):
        d = os.path.join(OUT, slug(t))
        os.makedirs(d, exist_ok=True)
        L, s0 = preamble(recipe(t)), 0
        for s in (1, 2, 3, 5):
            L += ['frames %d' % ((s - s0) * 60), 'shot %s/t%d.png' % (d, s)]
            s0 = s
        out = harness(L, os.path.join(d, 'script.txt'))
        open(os.path.join(d, 'run.log'), 'w').write(out)
        montage([os.path.join(d, 't%d.png' % s) for s in (1, 2, 3, 5)], ['%d s' % s for s in (1, 2, 3, 5)],
                'T%d  %s' % (t['n'], t['name']), os.path.join(d, 'strip.png'), '4x1')
        print('T%-2d %-14s %s' % (t['n'], t['name'], [l for l in out.splitlines() if l.startswith('wait_loaded')]))


def master(args):
    W.ensure_input()
    for t in by_n(args):
        d = os.path.join(OUT, slug(t))
        os.makedirs(d, exist_ok=True)
        frames = []
        for m in (0, 0.5, 1):
            f = os.path.join(d, 'master_%g.png' % m)
            harness(preamble(recipe(t, [('Trip Master', m)])) + ['frames 180', 'shot ' + f], os.path.join(d, 'master.txt'))
            frames.append(f)
        montage(frames, ['Trip Master %g' % m for m in (0, 0.5, 1)], 'T%d  %s' % (t['n'], t['name']),
                os.path.join(d, 'master.png'), '3x1')
        print(os.path.join(d, 'master.png'))


def contact():
    frames, labels = [], []
    for t in TRIPS:
        frames.append(os.path.join(OUT, slug(t), 't3.png'))
        labels.append('T%d %s' % (t['n'], t['name']))
    montage(frames, labels, 'W24 Trip looks (3 s)', os.path.join(OUT, 'contact.png'), '4x4')


def perf(args):
    for t in by_n(args):
        rows = []
        for label, e in (('base', recipe(dict(t, dials=[]))), ('trip', recipe(t))):
            out = harness(preamble(e) + ['frames 120', 'stats_reset', 'frames 180', 'stats %s' % label],
                          os.path.join(OUT, '_perf.txt'))
            m = re.search(r'GPU ms avg\s+([\d.]+)', out)
            rows.append(m.group(1) if m else '?')
        print('T%-2d %-14s base %s ms  trip %s ms  (GPU avg, 1080p)' % (t['n'], t['name'], rows[0], rows[1]))


def freeze():
    t = TRIPS[12]
    env = dict(os.environ, LOADEHR_SIM_STATS='1', LOADEHR_LOG_STDERR='1')
    out = harness(preamble(recipe(t, [('Time Warp', 1)])) + ['frames 120', 'set Freeze 1', 'frames 90', 'set Freeze 0', 'frames 60'],
                  os.path.join(OUT, '_freeze.txt'), env)
    for f, tot in re.findall(r'SIMAIR frame (\d+): .*total ([\d.]+)', out):
        print('sim frame %s  smoke total %s' % (f, tot))


def md():
    print('| # | Look | Base recipe | Trip dials (on top of the base) |\n|---|---|---|---|')
    for t in TRIPS:
        b = next(e for e in W.EXAMPLES if e['n'] == t['base'])
        print('| T%d | %s | %d %s | %s |' % (t['n'], t['name'], b['n'], b['name'], ', '.join(W.readable(t['dials']))))


if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    c, a = sys.argv[1], sys.argv[2:]
    {'run': run, 'master': master, 'contact': lambda a: contact(), 'perf': perf, 'freeze': lambda a: freeze(),
     'md': lambda a: md()}[c](a)
