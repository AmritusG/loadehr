#!/usr/bin/env python3
"""LoadEhr W2 Stage 4: the Deform group. Every deform alone on a model (front + three-quarter shots), everything at Deform
Amount 2, one deform on text; the cost of each (harness stats: plugin wall ms and GPU ms, 1080p, 120 frames) against the same
model with every deform at 0 (the plain programs). Jelly: a spin that starts and stops (the lag and the spring back).

usage: tools/w2_deform.py run <out> [case ...]    -> <out>/<case>_{front,side}.png, <out>/cost.md
       tools/w2_deform.py strips <out> <dst>"""
import os, re, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT = '/System/Library/Fonts/Supplemental/Arial.ttf'
SKULL = os.path.expanduser('~/Documents/Resolume Arena/MaskEhr Library/Skulls/Human Skull/scene.gltf')
MASK = os.path.expanduser('~/Documents/Resolume Arena/MaskEhr Library/Masks/Plague Mask/scene.gltf')
CASES = [  # name, dials, model ('text' = Model Source Text)
    ('none', [], SKULL),
    ('taper', [('Taper', 1.2)], SKULL),
    ('twist', [('Twist', 2.5)], SKULL),
    ('bend', [('Bend', 1.2)], SKULL),
    ('squash', [('Squash', 0.5)], SKULL),
    ('squash_beat', [('Squash Beat', 1.5)], SKULL),
    ('shear', [('Shear', 0.8)], SKULL),
    ('bulge', [('Bulge', 1.0)], SKULL),
    ('spherize', [('Spherize', 0.7)], SKULL),
    ('noise_warp', [('Noise Warp', 0.8), ('Warp Scale', 2.5), ('Warp Speed', 0.5)], SKULL),
    ('ripple', [('Ripple', 0.05), ('Ripple Freq', 20)], SKULL),
    ('melt', [('Melt', 0.5)], SKULL),
    ('glitch', [('Glitch Shift', 0.4), ('Glitch Bands', 16), ('Glitch Rate', 6)], SKULL),
    ('mirror_fold_x', [('Mirror Fold', 1)], SKULL),
    ('mirror_fold_xyz', [('Mirror Fold', 3)], SKULL),
    ('jelly', [('Jelly', 1.5)], SKULL),
    ('everything_x2', [('Deform Amount', 2), ('Taper', 0.6), ('Twist', 1.5), ('Bend', 0.6), ('Squash', 0.3), ('Shear', 0.4), ('Bulge', 0.5),
                       ('Spherize', 0.3), ('Noise Warp', 0.5), ('Ripple', 0.03), ('Glitch Shift', 0.2), ('Mirror Fold', 1), ('Jelly', 1)], SKULL),
    ('text_twist', [('Twist', 1.2), ('Deform Axis', 0), ('Bend', 0.4)], 'text'),
    ('text_noise', [('Noise Warp', 0.6), ('Warp Scale', 3)], 'text'),
]


def lines(name, dials, model, out):
    P = lambda s: os.path.join(out, '%s_%s.png' % (name, s))
    L = ['bundle %s/build/LoadEhrWow.bundle' % ROOT, 'size 1920 1080', 'instance', 'bpm 120']
    if model == 'text':
        L += ['mark', 'set Model Source 1', 'text Text LOADEHR', 'wait_loaded 60']
    else:
        L += ['text Model_A %s' % model, 'wait_loaded 60']
    L += ['set %s %g' % (k, v) for k, v in dials]
    if name == 'jelly':
        # spin up hard, then stop: the lag, then the wobble back
        L += ['set Spin Y 30', 'frames 40', 'composite arena', 'shot ' + P('front'), 'set Spin Y 0', 'frames 6', 'shot ' + P('side')]
    else:
        L += ['frames 30', 'composite arena', 'shot ' + P('front'), 'set Orbit Yaw 40', 'set Orbit Pitch 12', 'frames 2', 'shot ' + P('side'),
              'set Orbit Yaw 0', 'set Orbit Pitch 0']
    L += ['frames 10', 'stats_reset', 'frames 120', 'stats %s' % name]
    return L


def run(out, names):
    os.makedirs(out, exist_ok=True)
    rows = []
    for name, dials, model in CASES:
        if names and name not in names:
            continue
        sp = os.path.join(out, name + '.txt')
        open(sp, 'w').write('\n'.join(lines(name, dials, model, out)) + '\n')
        r = subprocess.run([os.path.join(ROOT, 'build', 'loadehr_harness'), sp], capture_output=True, text=True, cwd=ROOT)
        m = re.search(r'plugin wall ms avg\s+([\d.]+) p99\s+([\d.]+).*GPU ms avg\s+([\d.]+) p99\s+([\d.]+)', r.stdout)
        rows.append((name, m.groups() if m else ('?',) * 4))
        print(name, rows[-1][1], flush=True)
    base = next((float(r[1][2]) for r in rows if r[0] == 'none' and r[1][2] != '?'), None)
    md = ['| case | wall ms avg | p99 | GPU ms avg | p99 | GPU ms vs every deform 0 |', '|---|---|---|---|---|---|']
    for name, g in rows:
        d = '%+.2f' % ( float(g[2]) - base ) if base is not None and g[2] != '?' else '-'
        md.append('| %s | %s | %s | %s | %s | %s |' % (name, g[0], g[1], g[2], g[3], d))
    open(os.path.join(out, 'cost.md'), 'w').write('\n'.join(md) + '\n')
    print('\n'.join(md))


def strips(out, dst):
    os.makedirs(dst, exist_ok=True)
    args = []
    for name, _, _ in CASES:
        for s in ('front', 'side'):
            f = os.path.join(out, '%s_%s.png' % (name, s))
            if os.path.exists(f):
                args += ['-label', '%s %s' % (name, s), f]
    subprocess.run(['montage'] + args + ['-tile', '6x', '-geometry', '320x180+2+2', '-background', '#202020', '-fill', 'white', '-font', FONT,
                                         '-pointsize', '11', '-quality', '82', os.path.join(dst, 'deform_all.jpg')], check=True)
    print(os.path.join(dst, 'deform_all.jpg'))


if __name__ == '__main__':
    c, a = sys.argv[1], sys.argv[2:]
    if c == 'run':
        run(a[0], a[1:])
    elif c == 'strips':
        strips(a[0], a[1])
