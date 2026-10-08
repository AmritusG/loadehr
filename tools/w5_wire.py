#!/usr/bin/env python3
"""LoadEhr W5: the wireframe (Wire Mode / Back / Edges / Color / Glow / Bloom / Fade) in the harness.

usage: tools/w5_wire.py renders <out>   every Wire Mode x Wire Back x Wire Edges on the skull, the SciFi helmet, text "LOADEHR" and our
                                       SVG emblem, over the arena background -> <out>/<model>_<mode>_<back>_<edges>.png + renders.md
                                       (coverage = share of pixels with alpha > 0 in the raw frame)
       tools/w5_wire.py checks <out>    Wire Only fills nothing (Mix 0 -> coverage 0); Feature Edges on a cube (our generated
                                       test_models/gen/cube.obj): flagged edges + line coverage; Wire Glow 0 / 3 / 10 with Bloom; Wire
                                       Bloom alone changes nothing outside the halo -> <out>/checks.md
       tools/w5_wire.py cost <out>      harness 1080p GPU / plugin ms per mode, with Glow / Bloom -> <out>/cost.md"""
import os, re, struct, subprocess, sys, zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LIB = '/Users/amritrosell/Documents/Resolume Arena/MaskEhr Library'
SKULL = LIB + '/Skulls/Human Skull/scene.gltf'
HELMET = LIB + '/Helmets/SciFi Helmet/SciFiHelmet.gltf'
LOGO = os.path.join(ROOT, 'test_logos', 'test_emblem.svg')
CUBE = os.path.join(ROOT, 'test_models', 'gen', 'cube.obj')
LOG = os.path.expanduser('~/Library/Logs/LoadEhr/LoadEhr.log')
MODES = {1: 'over', 2: 'only', 3: 'dark'}


def png(path):
    """8-bit RGBA / RGB PNG (stb writes them) -> (w, h, RGBA bytes rows top-down; RGB gets alpha 255)"""
    d = open(path, 'rb').read()
    pos, idat, w = 8, b'', 0
    while pos < len(d):
        n, t = struct.unpack('>I4s', d[pos:pos + 8])
        if t == b'IHDR':
            w, h, bd, ct = struct.unpack('>IIBB', d[pos + 8:pos + 18])
            assert bd == 8 and ct in (2, 6), (bd, ct)
            bpp = 4 if ct == 6 else 3
        elif t == b'IDAT':
            idat += d[pos + 8:pos + 8 + n]
        pos += 12 + n
    raw, stride, out = zlib.decompress(idat), w * bpp, bytearray()
    prev = bytearray(stride)
    for y in range(h):
        f, row = raw[y * (stride + 1)], bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        if f == 1:
            for i in range(bpp, stride):
                row[i] = (row[i] + row[i - bpp]) & 255
        elif f == 2:
            for i in range(stride):
                row[i] = (row[i] + prev[i]) & 255
        elif f == 3:
            for i in range(stride):
                row[i] = (row[i] + ((row[i - bpp] if i >= bpp else 0) + prev[i]) // 2) & 255
        elif f == 4:
            for i in range(stride):
                a, b, c = (row[i - bpp] if i >= bpp else 0), prev[i], (prev[i - bpp] if i >= bpp else 0)
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                row[i] = (row[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        out += row
        prev = row
    if bpp == 3:
        rgba = bytearray(w * h * 4)
        rgba[0::4], rgba[1::4], rgba[2::4], rgba[3::4] = out[0::3], out[1::3], out[2::3], bytes([255]) * (w * h)
        out = rgba
    return w, h, bytes(out)


def coverage(path):
    w, h, px = png(path)
    return sum(1 for i in range(3, len(px), 4) if px[i] > 0) / (w * h)


def harness(lines, out, tag):
    sp = os.path.join(out, tag + '.txt')
    open(sp, 'w').write('\n'.join(lines) + '\n')
    mark = os.path.getsize(LOG) if os.path.exists(LOG) else 0
    r = subprocess.run([os.path.join(ROOT, 'build', 'loadehr_harness'), sp], capture_output=True, text=True, cwd=ROOT)
    return r.stdout, open(LOG, errors='replace').read()[mark:]


def load(model, size=(640, 360), bundle=None):
    bundle = bundle or ('LoadEhr' if model == 'logo' else 'LoadEhrWow')  # the Logo group is classic-only
    L = ['bundle %s/build/%s.bundle' % (ROOT, bundle), 'size %d %d' % size, 'instance']
    if model == 'text':
        L += ['set Model Source 1', 'mark', 'text Text LOADEHR', 'wait_loaded 60']
    elif model == 'logo':
        L += ['set Model Source 2', 'mark', 'text Logo_File ' + LOGO, 'wait_loaded 60']
    else:
        L += ['text Model_A ' + model, 'wait_loaded 60']
    return L + ['set Spin Y 0', 'set Rotate Y 25', 'set Rotate X -8']


def wire(mode, back=0, edges=0, mix=1.0, extra=()):
    return ['set Wire Mode %d' % mode, 'set Wireframe Mix %g' % mix, 'set Wire Back %d' % back, 'set Wire Edges %d' % edges] + \
           ['set %s %g' % kv for kv in extra]


MODELS = [('skull', SKULL), ('helmet', HELMET), ('text', 'text'), ('logo', 'logo')]


def renders(out):
    os.makedirs(out, exist_ok=True)
    rows = ['| model | mode | back | edges | coverage (raw alpha > 0) | shot |', '|---|---|---|---|---|---|']
    for name, m in MODELS:
        L = load(m) + ['set Wire Color 0.5', 'set Wire Sat 1', 'set Wire Bri 1', 'set Wire Color Mix 0.6', 'realtime 1.5']
        shots = []
        for mode in (1, 2, 3):
            for back in (0, 1):
                for edges in (0, 1):
                    tag = '%s_%s_%s_%s' % (name, MODES[mode], ('hidden', 'xray')[back], ('all', 'feature')[edges])
                    L += wire(mode, back, edges) + ['frames 3', 'composite arena', 'shot %s/%s.png' % (out, tag), 'shot_rgba %s/%s_raw.png' % (out, tag)]
                    shots.append((mode, back, edges, tag))
        harness(L, out, 'r_' + name)
        for mode, back, edges, tag in shots:
            rows.append('| %s | %s | %s | %s | %.4f | %s.png |' % (name, MODES[mode], ('Hidden', 'X-Ray')[back], ('All', 'Feature')[edges],
                                                                 coverage('%s/%s_raw.png' % (out, tag)), tag))
            os.remove('%s/%s_raw.png' % (out, tag))
        print('\n'.join(rows[-12:]), flush=True)
    open(os.path.join(out, 'renders.md'), 'w').write('\n'.join(rows) + '\n')


def cube():
    os.makedirs(os.path.dirname(CUBE), exist_ok=True)
    v = [(x, y, z) for x in (-1, 1) for y in (-1, 1) for z in (-1, 1)]
    f = [(1, 3, 4, 2), (5, 6, 8, 7), (1, 2, 6, 5), (3, 7, 8, 4), (1, 5, 7, 3), (2, 4, 8, 6)]
    with open(CUBE, 'w') as o:
        o.write('# LoadEhr W5 test cube (generated by tools/w5_wire.py)\n')
        o.writelines('v %d %d %d\n' % p for p in v)
        for a, b, c, d in f:
            o.write('f %d %d %d\nf %d %d %d\n' % (a, b, c, a, c, d))


def checks(out):
    os.makedirs(out, exist_ok=True)
    md = []
    # 1. Wire Only fills nothing: Mix 0 = no lines and no fill; Mix 1 = lines only, vs the solid
    md += ['## Wire Only fills nothing', '| model | solid (Over Surface, Mix 0) | Wire Only Mix 0 | Wire Only Mix 1, Hidden, All | ... Feature |', '|---|---|---|---|---|']
    for name, m in MODELS:
        L = load(m) + ['realtime 1.5']
        for tag, w in (('solid', wire(1, mix=0)), ('only0', wire(2, mix=0)), ('only1', wire(2)), ('onlyF', wire(2, 0, 1))):
            L += w + ['realtime 0.3' if tag == 'onlyF' else 'frames 3', 'shot_rgba %s/c_%s_%s.png' % (out, name, tag)]
        harness(L, out, 'c_' + name)
        c = [coverage('%s/c_%s_%s.png' % (out, name, t)) for t in ('solid', 'only0', 'only1', 'onlyF')]
        md.append('| %s | %.4f | %.4f | %.4f | %.4f |' % ((name,) + tuple(c)))
        print(md[-1], flush=True)
    # 2. Feature Edges on a cube: the flagged edges (log) and the drawn line coverage
    cube()
    L = load(CUBE) + ['set Rotate Y 35', 'set Rotate X -25', 'realtime 1'] + wire(2, 1, 0) + ['frames 3', 'shot_rgba %s/cube_all.png' % out, 'composite arena',
          'shot %s/cube_all_view.png' % out] + wire(2, 1, 1) + ['realtime 0.5', 'shot_rgba %s/cube_feature.png' % out, 'shot %s/cube_feature_view.png' % out]
    so, log = harness(L, out, 'cube')
    e = re.findall(r'WIRE edges: (\d+) triangles, (\d+) of (\d+) triangle edges', log)
    ca, cf = coverage(out + '/cube_all.png'), coverage(out + '/cube_feature.png')
    md += ['', '## Feature Edges on a cube (12 triangles: 12 cube edges + 6 face diagonals; X-Ray, every face seen)',
           'flags (worker log): %s of %s triangle edges drawn (each cube edge belongs to 2 triangles: 12 x 2 = 24 expected; the 6 '
           'diagonals x 2 = 12 left out)' % (e[-1][1] if e else '?', e[-1][2] if e else '?'),
           'line coverage All %.4f -> Feature %.4f (x%.2f; the diagonals are ~%.0f %% of the line length: 6 x 2.83 / (12 x 2 + 6 x 2.83))' %
           (ca, cf, cf / ca if ca else 0, 100 * 6 * 2.83 / (24 + 6 * 2.83))]
    print('\n'.join(md[-3:]), flush=True)
    # 3. Glow with the global Bloom: saturated pixels and the bloom's spill grow with Wire Glow
    md += ['', '## Wire Glow 0 / 3 / 10, Post Bloom 2 (Wire Only, pale cyan: Wire Sat 0.4, skull)',
           '| Wire Glow | white-hot pixels (every channel >= 250: the light past the colour) | mean brightness of the frame (bloom spill included) |', '|---|---|---|']
    L = load(SKULL) + ['realtime 1.5'] + wire(2, extra=(('Wire Color Mix', 1), ('Wire Color', 0.5), ('Wire Sat', 0.4), ('Bloom', 2))) + ['composite black']
    for g in (0, 3, 10):
        L += ['set Wire Glow %g' % g, 'frames 3', 'shot %s/glow_%g.png' % (out, g)]
    harness(L, out, 'glow')
    for g in (0, 3, 10):
        w, h, px = png('%s/glow_%g.png' % (out, g))
        sat = sum(1 for i in range(0, len(px), 4) if min(px[i], px[i + 1], px[i + 2]) >= 250)
        md.append('| %g | %d | %.2f |' % (g, sat, sum(px[i] + px[i + 1] + px[i + 2] for i in range(0, len(px), 4)) / (3 * w * h)))
    print('\n'.join(md[-3:]), flush=True)
    # 4. Wire Bloom alone: nothing changes outside the halo (the lines' coverage dilated by the halo's reach)
    L = load(SKULL) + ['realtime 1.5'] + wire(2, extra=(('Wire Bloom Size', 0.5),)) + ['frames 3', 'shot_rgba %s/bloom_0.png' % out,
                                                                                         'set Wire Bloom 3', 'frames 3', 'shot_rgba %s/bloom_3.png' % out]
    harness(L, out, 'bloom')
    w, h, a = png(out + '/bloom_0.png')
    _, _, b = png(out + '/bloom_3.png')
    reach = int((4 + 20 * 0.5) * (1 + 3 * 0.5) * (360 / 1080) * 2) + 2  # taps x stride (half size) x 2, in output px
    line = [a[i * 4 + 3] > 0 for i in range(w * h)]
    # dilate the line mask by `reach` (separable max over a square)
    rowm = [False] * (w * h)
    near = [False] * (w * h)
    xs = [[x for x in range(w) if line[y * w + x]] for y in range(h)]
    for y in range(h):
        for x in xs[y]:
            for xx in range(max(0, x - reach), min(w, x + reach + 1)):
                rowm[y * w + xx] = True
    for x in range(w):
        ys = [y for y in range(h) if rowm[y * w + x]]
        for y in ys:
            for yy in range(max(0, y - reach), min(h, y + reach + 1)):
                near[yy * w + x] = True
    outside = [i for i in range(w * h) if not near[i]]
    dmax = max((max(abs(a[i * 4 + c] - b[i * 4 + c]) for c in range(4)) for i in outside), default=0)
    inside = sum(1 for i in range(w * h) if near[i] and any(a[i * 4 + c] != b[i * 4 + c] for c in range(3)))
    md += ['', '## Wire Bloom 3 vs 0 (Wire Only, skull, Bloom Size 0.5, 640x360 raw frames)',
           'max difference OUTSIDE the halo reach (%d px round every line pixel, %d px outside): %d; pixels changed inside: %d' % (reach, len(outside), dmax, inside)]
    print('\n'.join(md[-2:]), flush=True)
    open(os.path.join(out, 'checks.md'), 'w').write('\n'.join(md) + '\n')


def cost(out):
    os.makedirs(out, exist_ok=True)
    cases = [('off (Mix 0)', wire(0, mix=0)), ('W1 Over Surface Mix 1 (plain)', wire(1)),
             ('Over Surface + colour (WIRE twin)', wire(1, extra=(('Wire Color Mix', 0.6),))), ('Over Surface X-Ray', wire(1, 1)),
             ('Wire Only Hidden', wire(2)), ('Wire Only X-Ray', wire(2, 1)), ('Wire Only Feature', wire(2, 0, 1)), ('Dark Fill Hidden', wire(3)),
             ('Dark Fill X-Ray', wire(3, 1)), ('Wire Only + Glow 3', wire(2, extra=(('Wire Glow', 3),))),
             ('Wire Only + Glow 3 + Bloom 3', wire(2, extra=(('Wire Glow', 3), ('Wire Bloom', 3))))]
    rows = ['| case | %s |' % ' | '.join('%s plugin ms avg / GPU ms avg' % n for n, _ in MODELS[:2]), '|---|---|---|']
    res = {}
    for name, m in MODELS[:2]:
        L = load(m, (1920, 1080)) + ['realtime 1.5']
        for i, (c, w) in enumerate(cases):
            L += ['set Wire Glow 0', 'set Wire Bloom 0', 'set Wire Color Mix 0'] + w + ['realtime 0.5', 'stats_reset', 'frames 120', 'stats C%d' % i]
        so, _ = harness(L, out, 'cost_' + name)
        for i in range(len(cases)):
            m2 = re.search(r'stats C%d\s.*?plugin wall ms avg\s+([\d.]+).*?GPU ms avg\s+([\d.]+)' % i, so)
            res[(name, i)] = '%s / %s' % m2.groups() if m2 else '?'
    for i, (c, _) in enumerate(cases):
        rows.append('| %s | %s | %s |' % (c, res[('skull', i)], res[('helmet', i)]))
    print('\n'.join(rows))
    open(os.path.join(out, 'cost.md'), 'w').write('\n'.join(rows) + '\n')


def combos(out):
    """what the wire combines with: Deform + Shatter (pieces keep their edges), Letters (Type On mid-way), clones (classic Copies),
    the morph chain mid-hop (skull -> helmet), Mirror Floor (draws the surface: not wired)"""
    os.makedirs(out, exist_ok=True)
    W = wire(2, 0, 1, extra=(('Wire Color Mix', 0.8), ('Wire Color', 0.5), ('Wire Sat', 0.8), ('Wire Glow', 3)))
    runs = [
        ('deform_shatter', load('text') + W + ['set Noise Warp 0.6', 'set Shatter Mode 1', 'realtime 0.5', 'pulse Shatter', 'realtime 0.8']),
        ('letters', load('text') + W + ['set Type On 0.6', 'set Letter Wave 0.5', 'realtime 1']),
        ('clones', load(SKULL, bundle='LoadEhr') + W + ['set Clone Layout 1', 'set Clone Count 6', 'set Clone Spacing 3', 'set Distance 9', 'realtime 1.5']),
        ('chain_midhop', load(SKULL) + ['mark', 'text Model_B ' + HELMET, 'wait_slots 1 60'] + W + ['set Transition 0.5', 'set Transition Style 0', 'realtime 1.5']),
        ('mirror_floor', load(SKULL) + W + ['set Mirror Floor 0.8', 'realtime 1']),
    ]
    rows = ['| combination | coverage | shot |', '|---|---|---|']
    for tag, L in runs:
        harness(L + ['frames 2', 'composite arena', 'shot %s/%s.png' % (out, tag), 'shot_rgba %s/%s_raw.png' % (out, tag)], out, tag)
        rows.append('| %s | %.4f | %s.png |' % (tag, coverage('%s/%s_raw.png' % (out, tag)), tag))
        os.remove('%s/%s_raw.png' % (out, tag))
        print(rows[-1], flush=True)
    open(os.path.join(out, 'combos.md'), 'w').write('\n'.join(rows) + '\n')


if __name__ == '__main__':
    {'renders': renders, 'checks': checks, 'cost': cost, 'combos': combos}[sys.argv[1]](sys.argv[2])
