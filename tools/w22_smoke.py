#!/usr/bin/env python3
"""W22: the 3D smoke look-book. Each example = a model + its non-default dials (THE recipe, also used for Arena and
docs/smoke_examples.md). Renders a 5 s harness sequence at 1080p (frames at 1, 2, 3, 5 s; fixed 1/60 s steps).
usage: w22_smoke.py run [n ...]      render examples (all if none) -> verify/w22/<nn>_<slug>/t{1,2,3,5}.png + strip.png
       w22_smoke.py contact          verify/w22/contact.png (the 3 s frame of each)
       w22_smoke.py perf [n ...]     GPU ms per example at 1080p (sanity only)
       w22_smoke.py md               print the recipe table (markdown)"""
import os, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# LoadEhr has no library. MaskEhr's look recipes were made on files that sit in MaskEhr's model folder; LoadEhr's renders point a
# Model slot at those FILES by path (like any other file on this Mac). LoadEhr never scans or lists the folder.
LIB = os.path.expanduser('~/Documents/Resolume Arena/MaskEhr Library')
OUT = os.path.join(ROOT, 'verify', 'w22')
FONT = '/System/Library/Fonts/Supplemental/Arial.ttf'
# MaskEhr's recipes were tuned on a mask that is FORBIDDEN in LoadEhr; LoadEhr's renders use the Plague Mask file instead (both sides)
MASK = LIB + '/Masks/Plague Mask/scene.gltf'

# Colours are HSBA quartets (one picker in Arena): (hue, sat, bri, alpha) under "<X> Color/Sat/Bri/Alpha".
def colour(prefix, h, s, b, a=1.0, first=None):
    return [(first or prefix + ' Color', h), (prefix + ' Sat', s), (prefix + ' Bri', b), (prefix + ' Alpha', a)]

EXAMPLES = [
    dict(n=1, name='Dragon Breath', model=MASK, dials=[
        ('Smoke Engine', 2), ('Haze', 0), ('Glow Cloud', 0), ('Smoke Amount', 4),
        ('Emit Eyes', 10), ('Eyes Heat', 2), ('Smoke Fade', 0.8), ('Vorticity', 3), ('Sim Box', 2),
        ('Orbit Yaw', 40), ('Orbit Pitch', 5), ('Distance', 5),
        *colour('Smoke', 0.07, 0.8, 1.0),
        ('A Enable', 1), ('A Type', 1), ('A Anchor To Mask', 0), ('A Pos X', -0.5), ('A Pos Y', 1.2), ('A Pos Z', 2),
        ('A Intensity', 40), *colour('A', 0.07, 0.7, 1.0),
        ('C Enable', 1), ('C Intensity', 4),
        *colour('C Left', 0.07, 0.85, 1.0, first='C Left Color'),
        ('Emit Offset', 0.15), ('Smoke Ambient', 0.3),  # W23
    ]),
    dict(n=2, name='Incense', model=LIB + '/Lanterns/Ding Censer/scene.gltf', dials=[
        ('Smoke Engine', 1), ('Haze', 0), ('Glow Cloud', 0), ('Smoke Amount', 3),
        ('Emit Inside', 6), ('Inside Heat', 3), ('Smoke Fade', 0.2), ('Buoyancy', 2.5),
        ('A Enable', 1), ('A Type', 1), ('A Pos Z', 0), ('A Intensity', 12), *colour('A', 0.08, 0.6, 1.0),
        ('B Enable', 1), ('B Type', 1), ('B Anchor To Mask', 1), ('B Pos X', 0.5), ('B Pos Y', 1.5), ('B Pos Z', 1.5),
        ('B Intensity', 10), *colour('B', 0.08, 0.4, 1.0),
        ('Smoke Ambient', 0.5),  # W23
    ]),
    dict(n=3, name='Fog Bank', model=MASK, dials=[
        ('Smoke Engine', 2), ('Haze', 0), ('Glow Cloud', 0),
        ('Emit Behind', 8), ('Buoyancy', 0.3), ('Wind X', 0.8), ('Sim Box', 3.5),
        ('A Enable', 1), ('A Pos Z', -2.5), ('A Intensity', 20), ('A Cone Angle', 70),
        *colour('A', 0.58, 0.6, 1.0),
        ('Key Intensity', 0.3), ('Env Intensity', 0.15),
        ('Smoke Ambient', 0.8),  # W23
    ]),
    dict(n=4, name='Smoulder', model=MASK, dials=[
        ('Smoke Engine', 2), ('Haze', 0), ('Glow Cloud', 0), ('Smoke Amount', 2),
        ('Emit Rim', 5), ('Rim Heat', 4), ('Smoke Fade', 1), *colour('Smoke', 0.06, 0.5, 1.0),
        ('Key Intensity', 0.8), ('Rim Glow', 2), *colour('Rim', 0.06, 0.8, 1.0),
        ('A Enable', 1), ('A Type', 1), ('A Anchor To Mask', 0), ('A Pos Y', 1), ('A Pos Z', -1.2), ('A Intensity', 4),
        *colour('A', 0.06, 0.7, 1.0),
        ('Emit Offset', 0.25), ('Smoke Ambient', 0.6),  # W23
    ]),
    dict(n=5, name='Ghost Whisper', model=LIB + '/Masks/Plague Mask/scene.gltf', dials=[
        ('Smoke Engine', 2), ('Haze', 0), ('Glow Cloud', 0), ('Smoke Amount', 4),
        ('Emit Eyes', 4), ('Eyes Heat', 0.6), ('Vorticity', 7), ('Buoyancy', 1),
        *colour('Smoke', 0.33, 0.35, 1.0),
        ('C Mirror X', 0), ('C Left Pos X', -0.13), ('C Left Pos Y', 0.21), ('C Left Pos Z', 0.55),
        ('C Right Pos X', 0.27), ('C Right Pos Y', 0.21), ('C Right Pos Z', 0.55),
        ('C Enable', 1), ('C Intensity', 3),
        *colour('C Left', 0.33, 0.45, 1.0, first='C Left Color'),
        ('A Enable', 1), ('A Type', 1), ('A Anchor To Mask', 0), ('A Pos Y', 1.5), ('A Pos Z', 2), ('A Intensity', 18),
        *colour('A', 0.33, 0.3, 1.0),
        ('Emit Offset', 0.1), ('Smoke Ambient', 1.5),  # W23
    ]),
    dict(n=6, name='Exhale On Beat', model=MASK, dials=[
        ('Smoke Engine', 1), ('Haze', 0), ('Glow Cloud', 0), ('Smoke Amount', 4),
        ('Emit Eyes', 6), ('Emit Behind', 1), ('Eyes Heat', 1), ('Beat Puff', 2), ('Puff Amount', 10), ('Smoke Fade', 1.2),
        ('C Enable', 1), ('C Intensity', 2),
        ('Orbit Yaw', 40), ('Orbit Pitch', 5), ('Distance', 5),
        ('A Enable', 1), ('A Type', 1), ('A Anchor To Mask', 0), ('A Pos X', 0.8), ('A Pos Y', 2), ('A Pos Z', 2.5),
        ('A Intensity', 40),
        ('Smoke Ambient', 0.8),  # W23
    ]),
    dict(n=7, name='Vortex', model=MASK, dials=[
        ('Smoke Engine', 2), ('Haze', 0), ('Glow Cloud', 0), ('Smoke Amount', 3),
        ('Spin Y', 12), ('Emit Behind', 8), ('Smoke Fade', 1), ('Vorticity', 6), ('Buoyancy', 0.8), ('Orbit Pitch', 25),
        ('A Enable', 1), ('A Type', 1), ('A Anchor To Mask', 0), ('A Pos X', 0.8), ('A Pos Y', 2), ('A Pos Z', 2.5),
        ('A Intensity', 25),
        ('Smoke Ambient', 0.4), ('Smoke Key Light', 1),  # W23
    ]),
    dict(n=8, name='Caged Smoke', model=LIB + '/Lanterns/Birdcage/scene.gltf', dials=[
        ('Smoke Engine', 1), ('Haze', 0), ('Glow Cloud', 0), ('Sim Res', 2), ('Smoke Amount', 1.5),
        ('Emit Inside', 3), ('Inside Heat', 4), ('Smoke Fade', 0.3), ('Wind X', 0.3), ('Orbit Pitch', 10),
        ('A Enable', 1), ('A Type', 1), ('A Pos Z', 0), ('A Intensity', 6),
        ('B Enable', 1), ('B Type', 1), ('B Pos X', 1), ('B Pos Y', 2.5), ('B Pos Z', 1.5), ('B Intensity', 20), *colour('B', 0.1, 0.3, 1.0),
        ('Smoke Ambient', 0.4),  # W23
    ]),
    dict(n=9, name='Skull Breath', model=LIB + '/Skulls/Human Skull/scene.gltf', dials=[
        ('Smoke Engine', 2), ('Haze', 0), ('Glow Cloud', 0),
        ('Emit Eyes', 5), ('Eyes Heat', 2.5), ('Smoke Fade', 1), ('Orbit Yaw', 20),
        *colour('Smoke', 0.0, 0.6, 1.0),
        ('C Enable', 1), ('C Intensity', 5),
        *colour('C Left', 0.0, 0.95, 1.0, first='C Left Color'),
        ('Emit Offset', 0.15), ('Smoke Ambient', 1.5), ('Smoke Amount', 3),  # W23
    ]),
    dict(n=10, name='Smoking Letters', model=None, dials=[
        ('Model Source', 1), ('Text', 'LOADEHR'),
        ('Metal Override', 1), ('Brightness', 0.35), ('Roughness Scale', 0.6),
        ('Smoke Engine', 2), ('Haze', 0), ('Glow Cloud', 0),
        ('Emit Rim', 6), ('Rim Heat', 4), ('Buoyancy', 3), ('Smoke Fade', 1.2), ('Smoke Amount', 2),
        ('A Enable', 1), ('A Pos Z', -2), ('A Intensity', 10), *colour('A', 0.58, 0.3, 1.0),
        ('Emit Offset', 0.2), ('Smoke Key Light', 1.5),  # W23
    ]),
    dict(n=11, name='Projected Smoke', model=MASK, fx=True, dials=[
        ('Smoke Engine', 1), ('Haze', 0), ('Glow Cloud', 0), ('Smoke Amount', 3),
        ('Emit Behind', 6), ('Emit Rim', 3), ('Project Input', 1),
        ('A Enable', 1), ('A Type', 1), ('A Anchor To Mask', 0), ('A Pos X', 0.8), ('A Pos Y', 2), ('A Pos Z', 2.5),
        ('A Intensity', 25),
        ('Smoke Ambient', 0.5),  # W23
    ]),
    dict(n=12, name='Audio Fog', model=MASK, fft=1.0, dials=[
        ('Smoke Engine', 2), ('Haze', 0), ('Glow Cloud', 0),
        ('Emit Behind', 1), ('Audio > Emit', 2), ('Buoyancy', 0.5), ('Smoke Fade', 1),
        ('A Enable', 1), ('A Pos Z', -2.5), ('A Intensity', 10), ('A Cone Angle', 70), *colour('A', 0.6, 0.35, 1.0),
        ('Smoke Ambient', 0.8),  # W23
    ]),
]

TEXT_PARAMS = {'Text', 'Font Name', 'Model A'}

def slug(e):
    return '%02d_%s' % (e['n'], e['name'].lower().replace(' ', '_'))

def script(e, out, perf=False):
    w, h = 1920, 1080
    L = ['bundle %s/build/%s.bundle' % (ROOT, 'LoadEhrWowFX' if e.get('fx') else 'LoadEhrWow'), 'size %d %d' % (w, h)]  # W9: the sim is Wow-only
    if e.get('fx'):
        L.append('input ' + e.get('input', os.path.join(OUT, 'input_gen.png')))
    L.append('bpm 100')  # off the whole second: shots land at different beat phases
    if e.get('fft') is not None:
        L.append('fft %g' % e['fft'])
    L.append('instance')
    sets = []
    for k, v in e['dials']:
        if k in TEXT_PARAMS:
            sets.append('text %s %s' % (k.replace(' ', '_'), v))
        else:
            sets.append('set %s %g' % (k, v))
    if e['model']:
        L += ['text Model_A ' + e['model'], 'wait_loaded 240']
        L += sets
    else:  # generated: the source + text first, then wait for the build
        L += sets[:2] + ['wait_loaded 240'] + sets[2:]
    L += ['pulse Sim Reset']
    if perf:
        L += ['frames 120', 'stats_reset', 'frames 180', 'stats %s' % e['name']]
        return '\n'.join(L) + '\n'
    t = 0
    for s in (1, 2, 3, 5):
        L += ['frames %d' % ((s - t) * 60), 'shot_rgba %s/t%d.png' % (out, s)]
        t = s
    return '\n'.join(L) + '\n'

def by_n(args):
    ns = [int(a) for a in args]
    return [e for e in EXAMPLES if not ns or e['n'] in ns]

def ensure_input():
    p = os.path.join(OUT, 'input_gen.png')
    os.makedirs(OUT, exist_ok=True)
    if not os.path.exists(p):  # stand-in for an Arena generator: soft colour field
        subprocess.run(['magick', '-size', '480x270', 'gradient:#ff2a6d-#05d9e8', '(', '-size', '480x270',
                        'gradient:#ffd319-#7a04eb', '-rotate', '90', '-resize', '480x270!', ')', '-compose', 'blend',
                        '-define', 'compose:args=50', '-composite', '-resize', '1920x1080!', p], check=True)

def run(args):
    os.makedirs(OUT, exist_ok=True)
    ensure_input()
    for e in by_n(args):
        d = os.path.join(OUT, slug(e))
        os.makedirs(d, exist_ok=True)
        sp = os.path.join(d, 'script.txt')
        open(sp, 'w').write(script(e, d))
        r = subprocess.run([os.path.join(ROOT, 'build', 'loadehr_harness'), sp], capture_output=True, text=True, cwd=ROOT)
        open(os.path.join(d, 'run.log'), 'w').write(r.stdout + r.stderr)
        loaded = [l for l in r.stdout.splitlines() if l.startswith('wait_loaded')]
        frames = []
        for s in (1, 2, 3, 5):  # over black, as Arena shows a layer on an empty comp
            f = os.path.join(d, 'v%d.jpg' % s)
            subprocess.run(['magick', os.path.join(d, 't%d.png' % s), '-alpha', 'off', '-quality', '92', f], check=True)
            frames.append(f)
        cmd = ['montage'] + sum([['-label', '%d s' % s, f] for s, f in zip((1, 2, 3, 5), frames)], []) + [
            '-tile', '4x1', '-geometry', '640x360+4+4', '-background', '#202020', '-fill', 'white', '-font', FONT, '-pointsize', '20',
            '-title', '%d  %s' % (e['n'], e['name']), os.path.join(d, 'strip.png')]
        subprocess.run(cmd, check=True)
        k = 1
        while os.path.exists(os.path.join(d, 'strip_v%d.png' % k)):
            k += 1
        subprocess.run(['cp', '-p', os.path.join(d, 'strip.png'), os.path.join(d, 'strip_v%d.png' % k)], check=True)
        with open(os.path.join(d, 'iter.log'), 'a') as f:
            f.write('v%d: %s\n' % (k, ', '.join('%s %s' % kv for kv in e['dials'])))
        print('%-18s v%d %s  rc %d' % (e['name'], k, loaded, r.returncode))

def contact():
    frames = []
    for e in EXAMPLES:
        frames += ['-label', '%d %s' % (e['n'], e['name']), os.path.join(OUT, slug(e), 'v3.jpg')]
    subprocess.run(['montage'] + frames + ['-tile', '4x3', '-geometry', '640x360+4+4', '-background', '#202020',
                    '-fill', 'white', '-font', FONT, '-pointsize', '22', os.path.join(OUT, 'contact.png')], check=True)

def perf(args):
    for e in by_n(args):
        sp = os.path.join(OUT, 'perf_%02d.txt' % e['n'])
        open(sp, 'w').write(script(e, OUT, perf=True))
        r = subprocess.run([os.path.join(ROOT, 'build', 'loadehr_harness'), sp], capture_output=True, text=True, cwd=ROOT)
        print('\n'.join(l for l in r.stdout.splitlines() if e['name'] in l))

OPTIONS = {'Smoke Engine': 'Noise|Sim|Sim + Detail', 'Sim Res': '48|64|96|128', 'Beat Puff': 'Off|Every Beat|Every 2|Every 4|Every 8',
           'A Type': 'Spot|Point', 'B Type': 'Spot|Point', 'Model Source': 'Model File|Text|Logo'}

def readable(dials):
    """Recipe dials as a person sets them: options by name, booleans On / Off, each HSBA quartet as one picker."""
    d, out, skip = dict(dials), [], set()
    for k, v in dials:
        if k in skip:
            continue
        if k.endswith(' Color'):
            base = k[:-6]
            skip |= {base + ' Sat', base + ' Bri', base + ' Alpha'}
            out.append('%s H %g S %g B %g A %g' % (k, v, d[base + ' Sat'], d[base + ' Bri'], d[base + ' Alpha']))
        elif k in OPTIONS:
            out.append('%s %s' % (k, OPTIONS[k].split('|')[int(v)]))
        elif k.endswith(('Enable', 'Anchor To Mask', 'Mirror X', 'Override')):
            out.append('%s %s' % (k, 'On' if v else 'Off'))
        else:
            out.append('%s %s' % (k, v))
    return out

def md():
    print('| # | Example | Model | Dials (non-default) |\n|---|---|---|---|')
    for e in EXAMPLES:
        m = 'Text' if not e['model'] else os.path.relpath(os.path.dirname(e['model']), LIB)
        if e.get('fx'):
            m += ' (LoadEhr FX on a generator)'
        print('| %d | %s | %s | %s |' % (e['n'], e['name'], m, ', '.join(readable(e['dials']))))

if __name__ == '__main__':
    c, a = sys.argv[1], sys.argv[2:]
    {'run': run, 'contact': lambda a: contact(), 'perf': perf, 'md': lambda a: md()}[c](a)
