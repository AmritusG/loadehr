#!/usr/bin/env python3
"""W25: the three weak Trip looks made crisp (Smoke Rings, Contours, Oil Slick) + the new W25 clips' recipes.
Renders like tools/w24_trip.py (reproducible, over the textured background) into verify/w25/<tag>/<nn>_<slug>/.
usage: w25_looks.py run <tag> [n ...]    t1 t2 t3 t5 + strip.png per look (n = 2 / 7 / 9 = W24 T2 / T7 / T9, 102 / 107 / 109 = W25)
       w25_looks.py pair <n> <m> [s]       before | after side by side at s seconds (default 3): verify/w25/<n>_vs_<m>.png
       w25_looks.py md                     the W25 looks as a markdown table"""
import os, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import w24_trip as T

ROOT = T.ROOT
colour = T.colour

W25 = [
    dict(n=109, name='Crisp Rings', base=1, dials=T.WHITE + T.DRAGON + [
        *colour('A', 0.07, 0.1, 1.0), ('Orbit Yaw', 35), ('Sim Res', 2), ('Smoke Fade', 1.5), ('Emit Eyes', 0),
        ('C Intensity', 1), ('Ring On Beat', 2), ('Ring From', 0), ('Ring Size', 0.5), ('Ring Speed', 3), ('Ring Smoke', 4)]),
    dict(n=107, name='Contour Lines', base=3, dials=T.WHITE + T.FOG + [
        *colour('A', 0.58, 0.1, 1.0), ('A Intensity', 2), ('Smoke Ambient', 0), ('Smoke Contours', 4), ('Contour Bands', 8),
        ('Band Sharpness', 0.6), ('Band Scroll', 0.3), ('Contour Style', 0), ('Line Glow', 1), ('Trip Palette', 0)]),
    dict(n=102, name='Oil Slick v2', base=3, dials=T.WHITE + T.FOG + [
        *colour('A', 0.58, 0.1, 1.0), ('A Intensity', 3), ('Smoke Ambient', 0.6), ('Thin Film', 1.4), ('Thin Film Thick', 1.6)]),
]
T.W.OPTIONS.update({'Contour Style': 'Lines|Bands'})


def find(n):
    return next(t for t in T.TRIPS + W25 if t['n'] == n)


def run(tag, ns):
    T.OUT = os.path.join(ROOT, 'verify', 'w25', tag)
    os.makedirs(T.OUT, exist_ok=True)
    T.W.ensure_input()
    for n in ns:
        t = find(n)
        d = os.path.join(T.OUT, T.slug(t))
        os.makedirs(d, exist_ok=True)
        L, s0 = T.preamble(T.recipe(t)), 0
        for s in (1, 2, 3, 5):
            L += ['frames %d' % ((s - s0) * 60), 'shot %s/t%d.png' % (d, s)]
            s0 = s
        out = T.harness(L, os.path.join(d, 'script.txt'))
        open(os.path.join(d, 'run.log'), 'w').write(out)
        T.montage([os.path.join(d, 't%d.png' % s) for s in (1, 2, 3, 5)], ['%d s' % s for s in (1, 2, 3, 5)],
                  '%s  %s' % (tag, t['name']), os.path.join(d, 'strip.png'), '4x1')
        print(d)


if __name__ == '__main__':
    c, a = sys.argv[1], sys.argv[2:]
    if c == 'run':
        run(a[0], [int(x) for x in a[1:]])
    elif c == 'md':
        for t in W25:
            print('| %s | %s |' % (t['name'], ', '.join(T.W.readable(t['dials']))))


# ---- ring probe: one Fire Ring, side view (Orbit Yaw 90: the ring travels across the screen) and front view ----
RING_BASE = [('Emit Eyes', 0), ('Ring On Beat', 0), ('Ring From', 0), ('Ring Size', 0.6), ('Ring Speed', 3), ('Ring Smoke', 4),
             ('Sim Res', 2), ('Smoke Fade', 1.5), ('C Intensity', 1)]
RING_TIMES = (0.25, 0.5, 1.0, 1.5)


def ringprobe(tag, extra):
    """extra = 'Name=value' dials; writes verify/w25/rings/<tag>/{side,front}_<t>.png (arena) + _a.png (rgba) + strip.png"""
    d = os.path.join(ROOT, 'verify', 'w25', 'rings', tag)
    os.makedirs(d, exist_ok=True)
    dials = [(k, float(v)) for k, v in (x.split('=') for x in extra)]
    shots = []
    for view, yaw in (('side', 90), ('front', 0)):
        t = dict(n=900, name='ring probe', base=1, dials=T.WHITE + T.DRAGON + [*colour('A', 0.07, 0.1, 1.0)] + RING_BASE +
                 [('Orbit Yaw', yaw), ('Orbit Pitch', 0)] + dials)
        L, t0 = T.preamble(T.recipe(t)) + ['frames 60', 'pulse Fire Ring'], 0.0
        for s in RING_TIMES:
            L += ['frames %d' % round((s - t0) * 60), 'shot %s/%s_%g.png' % (d, view, s), 'shot_rgba %s/%s_%g_a.png' % (d, view, s)]
            t0 = s
            shots.append('%s/%s_%g.png' % (d, view, s))
        open(os.path.join(d, view + '.log'), 'w').write(T.harness(L, os.path.join(d, view + '.txt')))
    T.montage(shots, ['%s %g s' % (v, s) for v in ('side', 'front') for s in RING_TIMES], 'rings: %s %s' % (tag, ' '.join(extra)),
              os.path.join(d, 'strip.png'), '4x2')
    print(os.path.join(d, 'strip.png'))


if __name__ == '__main__' and sys.argv[1] == 'ringprobe':
    ringprobe(sys.argv[2], sys.argv[3:])


# ---- saturation probe (Oil Slick): the smoke's own light (premultiplied rgba shot: smoke is additive, alpha 0) by
# brightness bucket. A pale core = saturation falling as the light gets brighter. Also the arena composite's core.
def satprobe(tag, n, extra=()):
    d = os.path.join(ROOT, 'verify', 'w25', 'sat', tag)
    os.makedirs(d, exist_ok=True)
    t = find(n)
    L = T.preamble(T.recipe(t, list(extra))) + ['frames 180', 'shot %s/t3.png' % d, 'shot_rgba %s/t3_a.png' % d]
    T.harness(L, os.path.join(d, 'script.txt'))
    txt = subprocess.run(['magick', '%s/t3_a.png' % d, '-alpha', 'off', '-resize', '25%', '-compress', 'none', 'ppm:-'],
                         capture_output=True, text=True).stdout.split()
    w, h, mx = int(txt[1]), int(txt[2]), float(txt[3])
    v = [float(x) / mx for x in txt[4:]]
    buckets = {}
    for i in range(w * h):
        r, g, b = v[3 * i:3 * i + 3]
        hi, lo = max(r, g, b), min(r, g, b)
        if hi < 0.05:
            continue
        k = min(int(hi * 5), 4)
        buckets.setdefault(k, []).append((hi - lo) / hi)
    rows = []
    for k in sorted(buckets):
        s = buckets[k]
        rows.append('V %.1f-%.1f: %5d px  mean saturation %.2f' % (k / 5, (k + 1) / 5, len(s), sum(s) / len(s)))
    open(os.path.join(d, 'sat.txt'), 'w').write('\n'.join(rows) + '\n')
    print('%s (%s)\n  ' % (tag, t['name']) + '\n  '.join(rows))


if __name__ == '__main__' and sys.argv[1] == 'satprobe':
    satprobe(sys.argv[2], int(sys.argv[3]), [(k, float(v)) for k, v in (x.split('=') for x in sys.argv[4:])])
