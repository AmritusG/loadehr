#!/usr/bin/env python3
"""LoadEhr W2 Stage 2: libtess2 out, Clipper2 + earcut in. The same text / logo specs built by a reference build (W1, libtess2)
and by this build, rendered front-on as a flat silhouette (Debug View Albedo: white where covered, over black), compared by
coverage (pixels that differ) and by triangle count (the GEN log line). The shapes must match; the triangulation may differ.

usage: tools/w2_text.py svgs                        write our test SVGs (test_logos/w2/: fill rules, nesting, self-intersection)
       tools/w2_text.py run <out> <ref build dir>    render every case with both builds -> <out>/{ref,new}/<case>.png + table
       tools/w2_text.py strips <out> <dst>          W1 | W2 | diff x8 per case -> <dst>/<case>.jpg"""
import os, re, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LOGOS = os.path.join(ROOT, 'test_logos')
W2 = os.path.join(LOGOS, 'w2')
FONT = '/System/Library/Fonts/Supplemental/Arial.ttf'

# our own test shapes (no third-party art): a pentagram under each fill rule, squares nested three deep, a self-crossing bow
STAR = ' '.join('%s%.2f,%.2f' % ('M' if i == 0 else 'L', 100 + 90 * __import__('math').sin(i * 4 * 3.14159265 / 5),
                                  100 - 90 * __import__('math').cos(i * 4 * 3.14159265 / 5)) for i in range(5)) + ' Z'
SVGS = {
    'star_evenodd.svg': '<path fill="#e0a020" fill-rule="evenodd" d="%s"/>' % STAR,
    'star_nonzero.svg': '<path fill="#20a0e0" fill-rule="nonzero" d="%s"/>' % STAR,
    # outer square, a hole, an island in the hole, a hole in the island (even-odd), plus a nonzero ring drawn as two paths
    'nested.svg': '<path fill="#c04040" fill-rule="evenodd" d="M10,10 H190 V190 H10 Z M40,40 H160 V160 H40 Z M70,70 H130 V130 H70 Z M90,90 H110 V110 H90 Z"/>',
    'selfcross.svg': '<path fill="#40c080" fill-rule="nonzero" d="M20,20 L180,180 L180,20 L20,180 Z"/>'
                     '<path fill="#8040c0" fill-rule="evenodd" d="M20,100 C60,10 140,190 180,100 C140,10 60,190 20,100 Z"/>',
}

TEXT = [  # case, text, extra dials
    ('holes', 'OAB8%@', []),
    ('bold', 'OAB8%@', [('Font Style', 11)]),
    ('italic', 'OAB8%@', [('Font Style', 8)]),
    ('spacing', 'OAB8', [('Letter Spacing', 0.3)]),
    ('rows', 'LOAD|EHR 2', []),
    ('cjk', '回國龍鬱', [('Font Name', 'Hiragino Sans')]),
    ('arial_file', 'Rg&Q@', [('Font File', FONT)]),
    ('loadehr', 'LOADEHR', []),
]
LOGO = [('svg_' + n[:-4], os.path.join(W2, n)) for n in sorted(SVGS)] + [
    ('svg_emblem', os.path.join(LOGOS, 'test_emblem.svg')), ('pdf_emblem', os.path.join(LOGOS, 'test_emblem.pdf')),
    ('png_traced', os.path.join(LOGOS, 'test_emblem.png'))]
TEXT_PARAMS = ('Text', 'Font Name', 'Font File', 'Logo File')


def svgs():
    os.makedirs(W2, exist_ok=True)
    for n, body in SVGS.items():
        open(os.path.join(W2, n), 'w').write('<svg xmlns="http://www.w3.org/2000/svg" width="200" height="200" viewBox="0 0 200 200">%s</svg>\n' % body)
        print(os.path.join(W2, n))


def case_lines(name, bundle_dir, shot, source, dials):
    L = ['bundle %s/LoadEhr.bundle' % bundle_dir, 'size 960 540', 'instance', 'set Model Source %d' % source, 'set Debug View 1',
         'set Extrude Depth 0.25', 'mark']
    for k, v in dials:
        L.append('text %s %s' % (k.replace(' ', '_'), v) if k in TEXT_PARAMS else 'set %s %g' % (k, v))
    L += ['wait_loaded 60', 'frames 4', 'composite black', 'shot %s' % shot, 'means 1', 'get Status A',
          'set Debug View 0', 'set Orbit Yaw 30', 'set Orbit Pitch 15', 'frames 4', 'composite arena', 'shot %s' % shot.replace('.png', '_lit.png')]
    return L


def cases():
    out = []
    for c, t, d in TEXT:
        out.append((c, 1, [('Text', t)] + d))
    for c, f in LOGO:
        out.append((c, 2, [('Logo File', f)]))
    return out


def run(out, ref):
    svgs()
    rows = []
    for side, d in (('ref', ref), ('new', os.path.join(ROOT, 'build'))):
        os.makedirs(os.path.join(out, side), exist_ok=True)
        for c, src, dials in cases():
            shot = os.path.join(out, side, c + '.png')
            sp = os.path.join(out, side, c + '.txt')
            open(sp, 'w').write('\n'.join(case_lines(c, d, shot, src, dials)) + '\n')
            r = subprocess.run([os.path.join(d, 'loadehr_harness'), sp], capture_output=True, text=True, cwd=ROOT, env=dict(os.environ, LOADEHR_LOG_STDERR='1'))
            log = r.stdout + r.stderr
            tris = re.search(r'GEN .*\| (\d+) tris, (\d+) letters/shapes', log)
            px = re.search(r'means frame .*\| (\d+) px', log)
            st = re.search(r"get Status A = '(.*)'", log)
            rows.append((side, c, int(tris.group(1)) if tris else -1, int(tris.group(2)) if tris else -1, int(px.group(1)) if px else -1, st.group(1) if st else '?'))
    cmp = os.path.join(out, 'compare.txt')
    open(cmp, 'w').write('\n'.join('compare %s %s' % (os.path.join(out, 'ref', c + x), os.path.join(out, 'new', c + x)) for c, _, _ in cases() for x in ('.png', '_lit.png')) + '\n')
    r = subprocess.run([os.path.join(ROOT, 'build', 'loadehr_harness'), cmp], capture_output=True, text=True, cwd=ROOT)
    diff = {}
    for l in r.stdout.splitlines():
        m = re.match(r'compare \S+/ref/(\S+?)(_lit)?\.png vs \S+: mean ([\d.]+) max (\d+) pixels>2 (\d+) of (\d+)', l)
        if m:
            diff[m.group(1) + (m.group(2) or '')] = (float(m.group(3)), int(m.group(4)), int(m.group(5)))
    by = {(s, c): (t, g, p, st) for s, c, t, g, p, st in rows}
    table = ['| case | W1 tris | W2 tris | shapes W1 / W2 | covered px W1 | W2 | silhouette px differing (>2/255) | share of covered | lit 3/4 view: mean / max / px>2 | status (W2) |',
             '|---|---|---|---|---|---|---|---|---|---|']
    for c, _, _ in cases():
        a, b = by.get(('ref', c)), by.get(('new', c))
        dd = diff.get(c, (0, 0, -1))
        share = dd[2] / max(1, a[2]) * 100 if a else 0
        lit = diff.get(c + '_lit', (0, 0, -1))
        table.append('| %s | %d | %d | %d / %d | %d | %d | %d | %.3f %% | %.4f / %d / %d | %s |' % (c, a[0], b[0], a[1], b[1], a[2], b[2], dd[2], share, lit[0], lit[1], lit[2], b[3]))
    open(os.path.join(out, 'table.md'), 'w').write('\n'.join(table) + '\n')
    print('\n'.join(table))


def strips(out, dst):
    os.makedirs(dst, exist_ok=True)
    for c, _, _ in cases():
        args = []
        for x, t in (('.png', 'silhouette'), ('_lit.png', 'lit')):
            a, b = os.path.join(out, 'ref', c + x), os.path.join(out, 'new', c + x)
            d = os.path.join(out, 'diff_' + c + x)
            subprocess.run(['magick', a, b, '-compose', 'difference', '-composite', '-evaluate', 'multiply', '8', '-alpha', 'off', d], check=True)
            args += ['-label', 'W1 (libtess2) ' + t, a, '-label', 'W2 (Clipper2 + earcut) ' + t, b, '-label', 'diff x8', d]
        subprocess.run(['montage'] + args + ['-tile', '3x2',
                        '-geometry', '480x270+2+2', '-background', '#202020', '-fill', 'white', '-font', FONT, '-pointsize', '12', '-title', c, '-quality', '85',
                        os.path.join(dst, c + '.jpg')], check=True)
        print(os.path.join(dst, c + '.jpg'))


if __name__ == '__main__':
    c, a = sys.argv[1], sys.argv[2:]
    if c == 'svgs':
        svgs()
    elif c == 'run':
        run(a[0], a[1])
    elif c == 'strips':
        strips(a[0], a[1])
