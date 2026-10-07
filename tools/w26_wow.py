#!/usr/bin/env python3
"""W26: the WOW effects (LoadEhr: MaskEhr's nine without the lasers). Each look = a model + dials (THE recipe: harness renders, the Arena clips (tools/w26_arena.py) and
docs/wow_examples.md use the same list). A look may start from a W22 smoke recipe (base=n) and add its dials (later win).
Renders are reproducible (W23): clocks zeroed, host time pinned; frames over the textured background (composite arena).
usage: w26_wow.py run <tag> [name ...]   t1 t2 t3 t5 + strip.png per look -> verify/w26/<tag>/<slug>/
       w26_wow.py perf [name ...]        GPU ms per look at 1080p (sanity only) and of the look with its effect off
       w26_wow.py md                     the looks as a markdown table
       w26_wow.py list"""
import os, re, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import w22_smoke as W

ROOT = W.ROOT
LIB = W.LIB
HARNESS = os.path.join(ROOT, 'build', 'loadehr_harness')
FONT = W.FONT
colour = W.colour
MASK = W.MASK
SKULL = LIB + '/Skulls/Human Skull/scene.gltf'

OPTIONS = {'Mirror Room': 'Off|Box|Corridor|Two Mirrors', 'LED Colors': 'Picker|Rainbow', 'Shatter Physics': 'Analytic|Bounce', 'Catcher Plane': 'Floor|Wall', 'Caustic Source': 'Glass Parts|Whole Mask', 'Crack On Beat': 'Off|Every Beat|Every 2|Every 4|Every 8', 'Swarm Count': 'Off|2k|5k|10k|20k|40k', 'Release On Beat': 'Off|Every Beat|Every 2|Every 4|Every 8', 'Bolt Path': 'Crawl|Eye To Eye|To Floor|To Clones', 'Strike On Beat': 'Off|Every Beat|Every 2|Every 4|Every 8', 'Transition Style': 'Dissolve|Shatter|Dust|Slice|Liquid', 'Liquid Look': 'Chrome|Paint', 'Fire Burst': 'Off|Every Beat|Every 2|Every 4|Every 8', 'Proj Source': 'Input|Image File', 'Transition Play': 'Off|Once On Trigger|Ping-Pong|Loop', 'Style Order': 'Fixed|Cycle|Random'}
W.OPTIONS.update(OPTIONS)
# pickers whose ALPHA is not "<base> Alpha"
ALPHA_NAMES = {'Mirror': 'Mirror Reflect', 'Lava Tint': 'Lava Tint Mix', 'Flame Tint': 'Flame Tint Mix', 'Liquid': 'Liquid Tint Mix'}

LOOKS = [
    # ---- Stage 2: projector
    dict(name='Projector Haze', stage=2, model=MASK, fx=True, input=ROOT + '/test_images/projector_test.png', effect=('Proj Enable', 0), dials=[
        ('Proj Enable', 1), ('Proj Source', 0), ('Proj Pos X', 0), ('Proj Pos Y', 0.6), ('Proj Pos Z', -4), ('Proj Throw', 24),
        ('Proj Intensity', 25), ('Proj Beam', 1.2), ('Haze', 0.25), ('Smoke Amount', 1.2), ('Smoke Speed', 0.25), ('Orbit Yaw', 15),
        ('Key Intensity', 1.5)]),
    dict(name='Projection Mapped', stage=2, model=SKULL, effect=('Proj Enable', 0), dials=[
        ('Proj Image', ROOT + '/test_images/projector_test.png'), ('Proj Enable', 1), ('Proj Pos X', 1.2), ('Proj Pos Y', 0.8), ('Proj Pos Z', 4.5), ('Proj Throw', 18), ('Proj Intensity', 60),
        ('Proj Surface', 1.5), ('Proj Beam', 0.6), ('Haze', 0.15), ('Smoke Amount', 0.8), ('Key Intensity', 0.6), ('Env Intensity', 0.4),
        ('Orbit Yaw', -25)]),
    # ---- Stage 3: fire
    dict(name='Eyes Of Fire', stage=3, model=SKULL, effect=('Fire', 0), dials=[
        ('Fire', 1), ('Smoke Engine', 2), ('Sim Res', 2), ('Emit Eyes', 5), ('Eyes Heat', 2), ('Emit Offset', 0.1), ('Burn Rate', 3),
        ('Flame Temp', 2400), ('Flame Height', 0.5), ('Flame Glow', 3), ('Fire Smoke', 0.4), ('Haze', 0), ('Glow Cloud', 0),
        ('Vorticity', 4), ('Emit Eyes', 4),
        ('Smoke Amount', 2), ('Key Intensity', 0.8), ('Env Intensity', 0.4), ('Fire Light', 3), ('Orbit Yaw', 15)]),
    dict(name='Burning Letters', stage=3, model=None, effect=('Fire', 0), dials=[
        ('Model Source', 1), ('Text', 'LOADEHR'), ('Metal Override', 1), ('Brightness', 0.35), ('Roughness Scale', 0.6),
        ('Fire', 1), ('Smoke Engine', 2), ('Sim Res', 2), ('Sim Box', 3), ('Emit Rim', 2.5), ('Rim Heat', 2), ('Emit Offset', 0.1), ('Flame Temp', 1800),
        ('Flame Height', 0.6), ('Vorticity', 4), ('Fire Smoke', 0.6), ('Haze', 0), ('Glow Cloud', 0), ('Smoke Amount', 2), ('Fire Burst', 2),
        ('Burst Amount', 4), ('Fire Light', 3), ('Key Intensity', 0.6)], times=(2, 5, 8, 12)),
    # ---- Stage 4: liquid morph (Transition dragged 0 -> 1 by hand in Arena; here a value per shot)
    dict(name='Liquid Morph', arena=[('Transition', 0.45)], stage=4, model=SKULL, effect=('Transition Style', 0), times=(0.5, 1, 1.5, 2, 2.5), dials=[
        ('Model B', LIB + '/Helmets/Diving Helmet/scene.gltf'), ('Transition Style', 4), ('Liquid Goo', 0.6), ('Liquid Look', 0),
        ('Liquid Wobble', 0.4), ('Orbit Yaw', 20), ('Transition', 0.3)],
        at={0.5: ['set Transition 0.3'], 1: ['set Transition 0.45'], 1.5: ['set Transition 0.6'], 2: ['set Transition 0.75'], 2.5: ['set Transition 0.9']}),
    dict(name='Liquid Paint', arena=[('Transition', 0.45)], stage=4, model=MASK, effect=('Transition Style', 0), times=(0.5, 1, 1.5, 2, 2.5), dials=[
        ('Model B', SKULL), ('Transition Style', 4), ('Liquid Goo', 0.8), ('Liquid Look', 1), ('Liquid Wobble', 0.6), ('Transition', 0.3)],
        at={0.5: ['set Transition 0.3'], 1: ['set Transition 0.45'], 1.5: ['set Transition 0.6'], 2: ['set Transition 0.75'], 2.5: ['set Transition 0.9']}),
    # ---- Stage 5: lightning
    dict(name='Lightning Eyes', stage=5, model=SKULL, effect=('Strike On Beat', 0), times=(0.55, 1.05, 1.55, 2.6), bpm=120, dials=[
        ('Bolt Path', 1), ('Strike On Beat', 1), ('Bolt Count', 2), ('Branching', 0.6), ('Jaggedness', 0.5), ('Bolt Life', 0.45),
        ('Bolt Glow', 3), *colour('Bolt', 0.62, 0.6, 1.0), ('Bolt Flash', 3), ('Smoke Amount', 1.2), ('Haze', 0.1),
        ('Key Intensity', 0.7), ('Env Intensity', 0.3)]),
    dict(name='Tesla Skull', stage=5, model=SKULL, effect=('Strike Rate', 0), dials=[
        ('Bolt Path', 0), ('Strike Rate', 5), ('Bolt Count', 2), ('Branching', 0.5), ('Jaggedness', 0.45), ('Bolt Life', 0.3),
        ('Bolt Glow', 3), *colour('Bolt', 0.55, 0.5, 1.0), ('Bolt Flash', 2), ('Key Intensity', 0.6), ('Env Intensity', 0.3),
        ('Orbit Yaw', -20), ('Spin Y', 6)]),
    # ---- Stage 6: firefly swarm
    dict(name='Firefly Face', stage=6, model=SKULL, effect=('Swarm Count', 0), times=(1, 2.2, 3.4, 4.6, 6), bpm=120, dials=[
        ('Swarm Count', 4), ('Mask Visible', 0), ('Release On Beat', 4), ('Release Time', 1.5), ('Attraction', 3), ('Flock', 1),
        ('Swarm Speed', 1), ('Firefly Size', 2), ('Firefly Glow', 3), ('Twinkle', 0.5), *colour('Firefly', 0.14, 0.75, 1.0),
        ('Spin Y', 6)]),
    dict(name='Firefly Halo', stage=6, model=MASK, effect=('Swarm Count', 0), dials=[
        ('Swarm Count', 3), ('Swarm Offset', 0.35), ('Attraction', 1.2), ('Flock', 1.5), ('Swarm Speed', 2), ('Firefly Size', 2.5),
        ('Firefly Glow', 4), *colour('Firefly', 0.5, 0.6, 1.0), ('Key Intensity', 1.2)]),
    # ---- Stage 7: lava cracks
    dict(name='Lava Cracks', arena=[('Crack On Beat', 4), ('Crack Time', 2.5)], stage=7, model=SKULL, effect=('Lava Cracks', 0), times=(0.6, 1.2, 2, 3.5), dials=[
        ('Lava Cracks', 1), ('Crack Time', 2.5), ('Crack Scale', 2.5), ('Crack Width', 0.35), ('Lava Temp', 1700), ('Lava Glow', 3),
        ('Heat Haze', 1), ('Key Intensity', 0.7), ('Env Intensity', 0.35), ('Brightness', 0.6), ('Orbit Yaw', 15)],
        start=['pulse Crack Start']),
    dict(name='Lava Split', arena=[('Crack Spread', 1)], stage=7, model=SKULL, effect=('Lava Cracks', 0), times=(1, 2, 3, 4), dials=[
        ('Lava Cracks', 1), ('Crack Time', 2), ('Crack Scale', 2), ('Crack Width', 0.4), ('Lava Temp', 1800), ('Lava Glow', 3),
        ('Lava Split', 0.7), ('Lava Light', 4), ('Heat Haze', 1), ('Haze', 0.25), ('Smoke Amount', 0.6), ('Key Intensity', 0.6),
        ('Env Intensity', 0.3), ('Brightness', 0.6), ('Orbit Yaw', 25), ('Spin Y', 5)], start=['pulse Crack Start']),
    # ---- Stage 8: glass caustics
    dict(name='Caustic Glass', stage=8, model=SKULL, effect=('Caustics', 0), dials=[
        ('Glass', 1), ('Glass IOR', 1.5), ('Caustics', 2), ('Caustic Source', 1), ('Caustic IOR', 1.5), ('Caustic Rainbow', 0.8),
        ('Caustic Sharp', 0.5), ('Caustic Ripple', 0.25), ('Caustic Floor', 1.2), ('Caustic Smoke', 0.4), ('Caustic Surface', 1),
        ('Key Elevation', 40), ('Key Azimuth', 70), ('Shadow Catcher', 0.5), ('Haze', 0.12), ('Spin Y', 6)]),
    dict(name='Caustic Crystal', stage=8, model=LIB + '/Stage Icons/Crystal Cluster/scene.gltf', effect=('Caustics', 0), dials=[
        ('Caustics', 2), ('Caustic Source', 1), ('Caustic IOR', 1.6), ('Caustic Rainbow', 1.5), ('Caustic Sharp', 0.4),
        ('Caustic Ripple', 0.15), ('Caustic Floor', 1.5), ('Caustic Smoke', 0.6), ('Key Elevation', 45), ('Key Azimuth', -60),
        ('Shadow Catcher', 0.5), ('Haze', 0.15), ('Spin Y', 5)]),
    # ---- Stage 9: shards that bounce
    dict(name='Bounce Shards', arena=[('Shatter On Beat', 4)], stage=9, model=SKULL, effect=('Shatter Physics', 0), times=(0.4, 1.0, 2.0, 2.6, 3.6), start=['pulse Shatter'],
         at={2.6: ['pulse Reform']}, dials=[
        ('Shatter Physics', 1), ('Bounciness', 0.55), ('Floor Friction', 0.3), ('Shard Count', 96), ('Shatter Force', 1.5),
        ('Shatter Gravity', 6), ('Shard Spin', 4), ('Reform Time', 1.2), ('Shadow Catcher', 0.6), ('Mirror Floor', 0.35),
        ('Orbit Yaw', 25)]),
    dict(name='Shard Wall', arena=[('Shatter On Beat', 4)], stage=9, model=MASK, effect=('Shatter Physics', 0), times=(0.4, 1.0, 2.0, 2.6, 3.6), start=['pulse Shatter'],
         at={2.6: ['pulse Reform']}, dials=[
        ('Shatter Physics', 1), ('Bounciness', 0.7), ('Floor Friction', 0.2), ('Shard Count', 128), ('Shatter Force', 3),
        ('Shatter Gravity', 4), ('Catcher Plane', 1), ('Wall Distance', 1.2), ('Shadow Catcher', 0.6), ('Orbit Yaw', 55), ('Orbit Pitch', 10)]),
    # ---- Stage 10: infinity mirror room
    dict(name='Mirror Room', stage=10, model=SKULL, effect=('Mirror Room', 0), dials=[
        ('Mirror Room', 1), ('Mirror Count', 3), ('Box Size', 1.5), ('Box Depth', 1.5), ('Mirror Color', 0.55), ('Mirror Sat', 0.12), ('Mirror Bri', 1.0), ('Mirror Reflect', 0.8),
        ('Mirror Gap', 1.2), ('LED Colors', 1), ('LED Speed', 0.25), ('Distance', 8), ('Orbit Yaw', 35), ('Orbit Pitch', 18),
        ('Spin Y', 6), ('Key Intensity', 2)]),
    dict(name='Infinity Corridor', stage=10, model=MASK, effect=('Mirror Room', 0), dials=[
        ('Mirror Room', 2), ('Mirror Count', 4), ('Box Size', 1.6), ('Box Depth', 1.6), ('Mirror Color', 0.6), ('Mirror Sat', 0.25), ('Mirror Bri', 1.0), ('Mirror Reflect', 0.8),
        ('Mirror Gap', 1), ('LED Colors', 1), ('LED Speed', 0.4), ('Orbit Yaw', 18), ('Orbit Pitch', 6)]),  # LoadEhr: MaskEhr's lasers removed
]


def slug(e):
    return e['name'].lower().replace(' ', '_')


def find(names):
    if not names:
        return LOOKS
    return [e for e in LOOKS if e['name'] in names or slug(e) in names]


def recipe_dials(e):
    d = []
    if 'base' in e:
        d += next(x for x in W.EXAMPLES if x['n'] == e['base'])['dials']
    return d + list(e['dials'])


def preamble(e, w=1920, h=1080):
    fx = e.get('fx', False)
    L = ['bundle %s/build/%s.bundle' % (ROOT, 'LoadEhrWowFX' if fx else 'LoadEhrWow'), 'size %d %d' % (w, h)]
    if fx:
        W.ensure_input()
        L.append('input ' + e.get('input', os.path.join(W.OUT, 'input_gen.png')))
    L += ['bpm %g' % e.get('bpm', 120), 'instance']
    if e.get('fft') is not None:
        L.insert(-1, 'fft %g' % e['fft'])
    sets = []
    for k, v in recipe_dials(e):
        if k in W.TEXT_PARAMS or k in ('Model B', 'Proj Image'):
            sets.append('text %s %s' % (k.replace(' ', '_'), v))
        else:
            sets.append('set %s %g' % (k, v))
    mb = [x for x in sets if x.startswith('text Model_B')]
    sets = [x for x in sets if not x.startswith('text Model_B')]
    for x in mb:  # Model B first, waited for (the transition needs both)
        L += [x, 'wait_loaded 240']
    if e.get('model'):
        L += ['text Model_A ' + e['model'], 'wait_loaded 240'] + sets
    else:
        L += sets[:2] + ['wait_loaded 240'] + sets[2:]
    # reproducible: the SDF / eye search land, then every clock starts at 0 with the host clock pinned
    L += ['realtime 1.5', 'advance 0 zero', 'hosttime 0', 'pulse Sim Reset'] + e.get('start', [])
    return L


def harness(lines, path, env=None):
    open(path, 'w').write('\n'.join(lines) + '\n')
    r = subprocess.run([HARNESS, path], capture_output=True, text=True, cwd=ROOT, env=env)
    return r.stdout + r.stderr


def montage(files, labels, title, dst, tile='4x1'):
    args = ['montage']
    for f, l in zip(files, labels):
        args += ['-label', l, f]
    subprocess.run(args + ['-tile', tile, '-geometry', '640x360+4+4', '-background', '#202020', '-fill', 'white', '-font', FONT,
                           '-pointsize', '20', '-title', title, dst], check=True)


def run(tag, names):
    out = os.path.join(ROOT, 'verify', 'w26', tag)
    for e in find(names):
        d = os.path.join(out, slug(e))
        os.makedirs(d, exist_ok=True)
        L, t0 = preamble(e), 0
        times = e.get('times', (1, 2, 3, 5))
        for i, s in enumerate(times):
            L += e.get('at', {}).get(s, [])
            L += ['frames %d' % round((s - t0) * 60), 'shot %s/t%d.png' % (d, i)]
            t0 = s
        log = harness(L, os.path.join(d, 'script.txt'))
        open(os.path.join(d, 'run.log'), 'w').write(log)
        montage([os.path.join(d, 't%d.png' % i) for i in range(len(times))], ['%g s' % s for s in times], '%s  %s' % (tag, e['name']),
                os.path.join(d, 'strip.png'), '%dx1' % len(times))
        print(d)


def perf(names):
    for e in find(names):
        for off in (False, True):
            L = preamble(e)
            if off and e.get('effect'):
                L.append('set %s %g' % e['effect'])
            L += ['frames 120', 'stats_reset', 'frames 240', 'stats %s%s' % (e['name'], ' (effect off)' if off else '')]
            out = harness(L, '/tmp/w26_perf.txt')
            print('\n'.join(l for l in out.splitlines() if l.startswith('stats')))


def readable(dials):
    """as W.readable, with each picker's own alpha name"""
    d, out, skip = dict(dials), [], set()
    for k, v in dials:
        if k in skip:
            continue
        if k.endswith(' Color'):
            base = k[:-6]
            an = ALPHA_NAMES.get(base, base + ' Alpha')
            skip |= {base + ' Sat', base + ' Bri', an}
            out.append('%s H %g S %g B %g A %g' % (k, v, d.get(base + ' Sat', 0), d.get(base + ' Bri', 1), d.get(an, 1)))
        elif k in W.OPTIONS:
            out.append('%s %s' % (k, W.OPTIONS[k].split('|')[int(v)]))
        elif isinstance(v, str):
            out.append('%s %s' % (k, os.path.relpath(v, LIB) if v.startswith(LIB) else os.path.basename(v)))
        elif k.endswith(('Enable', 'Anchor To Mask', 'Mirror X', 'Override', 'Visible')):
            out.append('%s %s' % (k, 'On' if v else 'Off'))
        else:
            out.append('%s %s' % (k, v))
    return out


def md():
    print('| Look | Stage | Model | Dials (non-default) |\n|---|---|---|---|')
    for e in LOOKS:
        m = 'Text' if not e.get('model') else os.path.relpath(os.path.dirname(e['model']), LIB)
        if e.get('fx'):
            m += ' (LoadEhr Wow FX on a generator)'
        extra = ' Press: %s.' % ', '.join(c.replace('pulse ', '') for c in e.get('start', [])) if e.get('start') else ''
        print('| %s | %d | %s | %s.%s |' % (e['name'], e['stage'], m, ', '.join(readable(recipe_dials(e))), extra))


if __name__ == '__main__':
    c, a = sys.argv[1], sys.argv[2:]
    if c == 'run':
        run(a[0], a[1:])
    elif c == 'perf':
        perf(a)
    elif c == 'md':
        md()
    elif c == 'list':
        for e in LOOKS:
            print(e['stage'], e['name'])
