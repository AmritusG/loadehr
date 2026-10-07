#!/usr/bin/env python3
"""W15 trail proof. usage (repo root): tools/w15_trails.py <out dir> [W14 bundle, for the before row]
Renders the same motion (Spin Y 30, Wobble 2 / 2, Trail Echoes 7, Delay 4, Offset X 0.02) with each trail look from a
fresh instance, plus the mask alone at the same moment, then measures the trail only (pixels the live mask does not
cover), row by row from the mask's right edge to the trail's end:
  gap px    = pixels inside that span whose alpha is < 8 (a hole between copies)
  steps     = alpha jumps > 10 between neighbouring pixels (the copy edges: a stair of stamps)
  edge      = mean |d alpha / dx| in the older half of the trail (lower = softer)
  holes     = share of pixels in the older half with alpha < 8 (dust breaks the tail into grain)
The numbers sum frames f00-f04 (gap, steps) or average them (edge, holes).
Also `diffs` per config: the per-frame change (a stamp every Delay frames shows as a spike every 4th frame)."""
import os, struct, subprocess, sys, zlib, statistics

MODEL = os.path.abspath('test_models/gen/mask/mask.obj')  # LoadEhr: our generated mask (tools/gen_test_models.py)
MOTION = [('Spin Y', 30), ('Wobble Amount', 2), ('Wobble Speed', 2), ('Trail Echoes', 7), ('Trail Delay', 4), ('Trail Offset X', 0.02)]
CONFIGS = [
    ('echo_s0', []),
    ('echo_smear1', [('Trail Smear', 1)]),
    ('echo_blur08', [('Trail Blur', 0.8)]),
    ('echo_dust1', [('Trail Dust', 1)]),
    ('echo_all', [('Trail Smear', 1), ('Trail Blur', 0.5), ('Trail Dust', 0.6), ('Trail Glow', 2), ('Trail Color Mix', 0.5)]),
]


def read_png(path):
    data = open(path, 'rb').read()
    pos, idat, w = 8, b'', 0
    while pos < len(data):
        n, typ = struct.unpack('>I4s', data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + n]
        if typ == b'IHDR':
            w, h, depth, ctype = struct.unpack('>IIBB', body[:10])
            assert depth == 8 and ctype == 6, 'RGBA8 only'
        elif typ == b'IDAT':
            idat += body
        pos += 12 + n
    raw, bpp, stride = zlib.decompress(idat), 4, w * 4
    out, prev, i = [], bytearray(stride), 0
    for _ in range(h):
        f, line = raw[i], bytearray(raw[i + 1:i + 1 + stride])
        i += 1 + stride
        for x in range(stride):
            a = line[x - bpp] if x >= bpp else 0
            b = prev[x]
            c = prev[x - bpp] if x >= bpp else 0
            if f == 1: line[x] = (line[x] + a) & 255
            elif f == 2: line[x] = (line[x] + b) & 255
            elif f == 3: line[x] = (line[x] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        out.append(line)
        prev = line
    return w, h, out


def measure(trail_png, mask_png):
    w, h, t = read_png(trail_png)
    _, _, m = read_png(mask_png)
    gap = steps = 0
    edge, holes, older = [], 0, 0
    for y in range(h):
        mr, tr = m[y], t[y]
        right = max((x for x in range(w) if mr[4 * x + 3] > 0), default=-1)
        if right < 0:
            continue
        a = [tr[4 * x + 3] if mr[4 * x + 3] == 0 else 255 for x in range(w)]
        end = max((x for x in range(right + 1, w) if a[x] >= 8), default=-1)
        if end < 0:
            continue
        span = range(right + 1, end + 1)
        gap += sum(1 for x in span if a[x] < 8)
        steps += sum(1 for x in span if x > right + 1 and abs(a[x] - a[x - 1]) > 10)
        mid = (right + 1 + end) // 2
        for x in range(mid + 1, end + 1):
            edge.append(abs(a[x] - a[x - 1]))
            older += 1
            holes += a[x] < 8
    return gap, steps, statistics.mean(edge) if edge else 0.0, holes / max(1, older)


def render(bundle, configs, out, tag):
    lines = ['bundle ' + bundle, 'size 960 540']
    for name, extra in configs:
        lines += ['instance', 'text Model_A ' + MODEL, 'wait_loaded 60']
        params = [p for p in MOTION if extra is not None or not p[0].startswith('Trail')] + (extra or [])
        lines += ['set %s %g' % p for p in params] + ['frames 60']
        for k in range(5):
            lines += (['frames 1'] if k else []) + ['shot_rgba %s/%s_f%02d.png' % (out, name, k), 'shot %s/%s_f%02d_view.png' % (out, name, k)]
        lines += ['stats_reset', 'diffs 60', 'stats %s' % name, 'delete']
    script = os.path.join(out, 'trails_%s.txt' % tag)
    open(script, 'w').write('\n'.join(lines) + '\n')
    res = subprocess.run(['./build/loadehr_harness', script], capture_output=True, text=True)
    open(os.path.join(out, 'trails_%s.log' % tag), 'w').write(res.stdout + res.stderr)
    diffs = [list(map(float, l.split()[1:])) for l in res.stdout.splitlines() if l.startswith('diffs:')]
    gpu = [l.split('GPU ms avg')[1].split()[0] for l in res.stdout.splitlines() if l.startswith('stats ')]
    return {n: (d, g) for (n, _), d, g in zip(configs, diffs, gpu)}


def main():
    out = sys.argv[1]
    old = sys.argv[2] if len(sys.argv) > 2 else None  # the W14 bundle (before)
    os.makedirs(out, exist_ok=True)
    info = render('build/LoadEhr.bundle', CONFIGS + [('mask_only', None)], out, 'after')
    rows = [n for n, _ in CONFIGS]
    if old:
        info.update(render(old, [('before_w14', [])], out, 'before'))
        rows.insert(0, 'before_w14')
    print('%-14s %8s %7s %7s %7s | %s' % ('trail', 'gap px', 'steps', 'edge', 'holes', 'per-frame change: every 4th / others (median), GPU ms'))
    for name in rows:
        g = s = 0
        e, hl = [], []
        for k in range(5):
            r = measure('%s/%s_f%02d.png' % (out, name, k), '%s/mask_only_f%02d.png' % (out, k))
            g, s = g + r[0], s + r[1]
            e.append(r[2]), hl.append(r[3])
        d, gpu = info[name]
        fourth = [d[i] for i in range(len(d)) if i % 4 == 0]
        other = [d[i] for i in range(len(d)) if i % 4]
        print('%-14s %8d %7d %7.2f %7.3f | %.2f / %.2f, GPU %s' % (name, g, s, statistics.mean(e), statistics.mean(hl), statistics.median(fourth), statistics.median(other), gpu))


main()
