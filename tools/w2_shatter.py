#!/usr/bin/env python3
"""LoadEhr W2 Stage 3: text and logos shatter into real solid pieces. Renders (harness, by param name) of each case at rest,
mid-flight front + side, and fully reformed; the reformed frame must equal the rest frame bit for bit (the whole mesh draws again).

usage: tools/w2_shatter.py run <out>        every case -> <out>/<case>_*.png + <out>/report.txt (reform vs rest compares)
       tools/w2_shatter.py strips <out> <dst>   rest | flight front | flight side | reformed per case -> <dst>/<case>.jpg"""
import os, re, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LOGOS = os.path.join(ROOT, 'test_logos')
GEN = os.path.join(ROOT, 'test_models', 'gen')
FONT = '/System/Library/Fonts/Supplemental/Arial.ttf'
SOFT = [('Shatter Force', 1.0), ('Shatter Gravity', 1.2), ('Shard Spin', 2.0)]

# case: (bundle, source, text/logo, dials, flight frames)
CASES = [
    ('loadehr_pieces_64', 'LoadEhr', 1, 'LOADEHR', [('Shatter Mode', 1), ('Shard Count', 64)] + SOFT, 40),
    ('loadehr_pieces_8', 'LoadEhr', 1, 'LOADEHR', [('Shatter Mode', 1), ('Shard Count', 8)] + SOFT, 40),
    ('loadehr_pieces_256', 'LoadEhr', 1, 'LOADEHR', [('Shatter Mode', 1), ('Shard Count', 256)] + SOFT, 40),
    ('loadehr_letters', 'LoadEhr', 1, 'LOADEHR', [('Shatter Mode', 0), ('Shard Count', 64)] + SOFT, 40),
    ('loadehr_pieces_letters', 'LoadEhr', 1, 'LOADEHR', [('Shatter Mode', 2), ('Shard Count', 64)] + SOFT, 40),
    ('two_rows', 'LoadEhr', 1, 'LOAD|EHR', [('Shatter Mode', 1), ('Shard Count', 64)] + SOFT, 40),
    ('svg_logo', 'LoadEhr', 2, os.path.join(LOGOS, 'test_emblem.svg'), [('Shatter Mode', 1), ('Shard Count', 64)] + SOFT, 40),
    ('pdf_logo', 'LoadEhr', 2, os.path.join(LOGOS, 'test_emblem.pdf'), [('Shatter Mode', 1), ('Shard Count', 64)] + SOFT, 40),
    ('svg_bounce', 'LoadEhr', 2, os.path.join(LOGOS, 'test_emblem.svg'), [('Shatter Mode', 1), ('Shard Count', 64), ('Shatter Physics', 1),
                                                                              ('Bounciness', 0.55), ('Shadow Catcher', 0.6)] + SOFT, 90),
    ('text_bounce', 'LoadEhrWow', 1, 'LOADEHR', [('Shatter Mode', 1), ('Shard Count', 64), ('Shatter Physics', 1), ('Bounciness', 0.55),
                                                 ('Physics Speed', 1), ('Shadow Catcher', 0.6), ('Orbit Pitch', 12)] + SOFT, 90),
    # animated letters: the pieces ride their letter's wave until the shatter (reformed != rest: the wave moved on)
    ('text_letters_wave', 'LoadEhr', 1, 'LOADEHR', [('Shatter Mode', 1), ('Shard Count', 64), ('Letter Wave', 1), ('Wave Speed', 0.5)] + SOFT, 30),
    ('core_glow', 'LoadEhr', 1, 'OB', [('Shatter Mode', 1), ('Shard Count', 24), ('Tint Color', 0.11), ('Tint Sat', 0.7), ('Tint Mix', 1),
                                         ('Metal Override', 1), ('Metallic Value', 1), ('Part 5 Hue Shift', 0.95), ('Part 5 Glow', 2.5),
                                         ('Shatter Force', 0.4), ('Shatter Gravity', 0.3), ('Shard Spin', 1.5), ('Distance', 2.4)], 25),
]


# the Shatter transition from the text in slot A to a model in slot B (a generated test model), scrubbed by hand
TRANS = ('trans_text_to_model', 'LoadEhr', 1, 'LOADEHR', [('Shatter Mode', 1), ('Shard Count', 64), ('Transition Style', 1), ('Trans Spread', 0.6)], 0)


def trans_lines(out):
    name, bundle, src, what, dials, _ = TRANS
    P = lambda s: os.path.join(out, '%s_%s.png' % (name, s))
    L = ['bundle %s/build/%s.bundle' % (ROOT, bundle), 'size 960 540', 'instance', 'set Model Source 1']
    L += ['set %s %g' % (k, v) for k, v in dials]
    L += ['mark', 'text Text %s' % what, 'text Model_B %s' % os.path.join(GEN, 'knot', 'knot.obj'), 'wait_slots 2 60', 'frames 20', 'composite arena']
    for t in (0, 0.2, 0.35, 0.5, 0.65, 1):
        L += ['set Transition %g' % t, 'frames 2', 'shot ' + P('t%03d' % round(t * 100))]
    L += ['set Transition 0', 'frames 2', 'shot ' + P('back0'), 'compare %s %s' % (P('t000'), P('back0'))]
    return L


def lines(case, out):
    name, bundle, src, what, dials, fly = case
    P = lambda s: os.path.join(out, '%s_%s.png' % (name, s))
    L = ['bundle %s/build/%s.bundle' % (ROOT, bundle), 'size 960 540', 'instance', 'set Model Source %d' % src]
    for k, v in dials:
        L.append('set %s %g' % (k, v))
    L += ['mark', ('text Text %s' if src == 1 else 'text Logo_File %s') % what, 'wait_loaded 60', 'get Status A', 'frames 20',
          'composite arena', 'shot ' + P('rest'), 'pulse Shatter', 'frames %d' % fly, 'shot ' + P('fly'), 'set Orbit Yaw 65', 'frames 1',
          'shot ' + P('side'), 'set Orbit Yaw 0', 'pulse Reform', 'frames 240', 'shot ' + P('reformed'),
          'compare %s %s' % (P('rest'), P('reformed'))]
    return L


def run(out):
    os.makedirs(out, exist_ok=True)
    rep = []
    for c in CASES:
        sp = os.path.join(out, c[0] + '.txt')
        open(sp, 'w').write('\n'.join(lines(c, out)) + '\n')
        r = subprocess.run([os.path.join(ROOT, 'build', 'loadehr_harness'), sp], capture_output=True, text=True, cwd=ROOT,
                           env=dict(os.environ, LOADEHR_LOG_STDERR='1'))
        log = r.stdout + r.stderr
        gen = ([m for m in re.finditer(r'GEN .*\| (.*) \| outline', log)] or [None])[-1]
        cmp = re.search(r'compare \S+ vs \S+: (mean .*)', log)
        rep.append('%-24s %s | reformed vs rest: %s' % (c[0], gen.group(1) if gen else '?', cmp.group(1) if cmp else 'NO COMPARE'))
        print(rep[-1], flush=True)
    sp = os.path.join(out, TRANS[0] + '.txt')
    open(sp, 'w').write('\n'.join(trans_lines(out)) + '\n')
    r = subprocess.run([os.path.join(ROOT, 'build', 'loadehr_harness'), sp], capture_output=True, text=True, cwd=ROOT)
    cmp = re.search(r'compare \S+ vs \S+: (mean .*)', r.stdout)
    rep.append('%-24s transition 0 -> 1 -> 0: back at 0 vs first 0: %s' % (TRANS[0], cmp.group(1) if cmp else 'NO COMPARE'))
    print(rep[-1])
    open(os.path.join(out, 'report.txt'), 'w').write('\n'.join(rep) + '\n')


def strips(out, dst):
    os.makedirs(dst, exist_ok=True)
    for c in CASES:
        args = []
        for s, t in (('rest', 'rest'), ('fly', 'mid-flight front'), ('side', 'mid-flight side'), ('reformed', 'reformed (= rest)')):
            args += ['-label', t, os.path.join(out, '%s_%s.png' % (c[0], s))]
        subprocess.run(['montage'] + args + ['-tile', '4x1', '-geometry', '480x270+2+2', '-background', '#202020', '-fill', 'white', '-font', FONT,
                                             '-pointsize', '12', '-title', c[0], '-quality', '85', os.path.join(dst, c[0] + '.jpg')], check=True)
        print(os.path.join(dst, c[0] + '.jpg'))
    args = []
    for t in ('t000', 't020', 't035', 't050', 't065', 't100'):
        args += ['-label', 'Transition ' + t[1:], os.path.join(out, '%s_%s.png' % (TRANS[0], t))]
    subprocess.run(['montage'] + args + ['-tile', '6x1', '-geometry', '320x180+2+2', '-background', '#202020', '-fill', 'white', '-font', FONT,
                                         '-pointsize', '11', '-title', TRANS[0], '-quality', '85', os.path.join(dst, TRANS[0] + '.jpg')], check=True)


if __name__ == '__main__':
    c, a = sys.argv[1], sys.argv[2:]
    if c == 'run':
        run(a[0])
    elif c == 'strips':
        strips(a[0], a[1])
