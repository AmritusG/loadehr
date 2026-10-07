#!/usr/bin/env python3
"""LoadEhr W1: the MORPH CHAIN tests (harness, our generated models: tools/gen_test_models.py).
usage: tools/chain_test.py styles <out>    2 / 3 / 4 filled slots x every style (+ Chain Loop), Transition scrubbed 0 -> 1 in 9
                                           steps -> <out>/styles_<n><loop>_<style>.png rows + <out>/styles.png
       tools/chain_test.py play <out>      Transition Play Once / Ping-Pong / Loop / Loop + Chain Loop over 4 slots (Time 1 s, Hold
                                           0.5 s): which model shows, frame by frame (mean colour -> nearest model) -> play.txt
       tools/chain_test.py scrub <out>     every style: the slider walked forward, then backward over the same points: max diff
       tools/chain_test.py cost <out>      1080p, 4 slots, each style: wall ms resting vs mid-hop (Wow bundle, harness stats)
       tools/chain_test.py load <out>      4 heavy models loaded while the chain plays (realtime): frame times, swap / upload log
       tools/chain_test.py all <out>
The models (one colour each): A knot (gold), B mask (ivory), C crystal (violet), D star (red)."""
import os, re, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HARNESS = os.path.join(ROOT, 'build', 'loadehr_harness')
GEN = os.path.join(ROOT, 'test_models', 'gen')
MODELS = [GEN + '/knot/knot.obj', GEN + '/mask/mask.obj', GEN + '/crystal/crystal.obj', GEN + '/star/star.obj']
STYLES = ['Dissolve', 'Shatter', 'Dust', 'Slice', 'Liquid']
FONT = '/System/Library/Fonts/Supplemental/Arial.ttf'
# heavy files on this Mac for the memory / load test (MaskEhr's model folder, by path; LoadEhr never scans it)
HEAVY_DIR = os.path.expanduser('~/Documents/Resolume Arena/MaskEhr Library')
HEAVY = [HEAVY_DIR + '/Helmets/Corinthian Helmet', HEAVY_DIR + '/Crowns/Crown 1918', HEAVY_DIR + '/Helmets/Kabuto', HEAVY_DIR + '/Masks/Ganesha Mask']


def model_file(folder):
    for f in sorted(os.listdir(folder)):
        if f.lower().endswith(('.gltf', '.glb', '.fbx', '.obj')):
            return os.path.join(folder, f)
    raise SystemExit('no model in ' + folder)


def load(n, models=MODELS):
    L = ['mark'] + ['text Model_%s %s' % ('ABCD'[i], models[i]) for i in range(n)] + ['wait_slots %d 240' % n]
    return L


def harness(lines, path):
    open(path, 'w').write('\n'.join(lines) + '\n')
    r = subprocess.run([HARNESS, path], capture_output=True, text=True, cwd=ROOT)
    return r.stdout + r.stderr


def montage(files, labels, title, dst, tile, geo='320x180+2+2'):
    args = ['montage']
    for f, l in zip(files, labels):
        args += ['-label', l, f]
    subprocess.run(args + ['-tile', tile, '-geometry', geo, '-background', '#202020', '-fill', 'white', '-font', FONT, '-pointsize', '14',
                           '-title', title, dst], check=True)


def styles(out):
    os.makedirs(out, exist_ok=True)
    rows = []
    for n, loop in ((2, 0), (3, 0), (4, 0), (4, 1), (2, 1)):
        for s, name in enumerate(STYLES):
            d = os.path.join(out, 'st_%d%s_%s' % (n, 'L' if loop else '', name.lower()))
            os.makedirs(d, exist_ok=True)
            L = ['bundle build/LoadEhrWow.bundle', 'size 640 360', 'instance'] + load(n) + [
                'set Transition Style %d' % s, 'set Chain Loop %d' % loop, 'set Liquid Wobble 0', 'realtime 1.5', 'advance 0 zero', 'hosttime 0']
            xs = [i / 8 for i in range(9)]
            for i, x in enumerate(xs):
                L += ['set Transition %.6f' % x, 'frames %d' % (40 if s == 4 and i == 0 else 4), 'shot %s/x%d.png' % (d, i)]
            log = harness(L, os.path.join(d, 'script.txt'))
            open(os.path.join(d, 'run.log'), 'w').write(log)
            row = os.path.join(out, 'styles_%d%s_%s.png' % (n, 'L' if loop else '', name.lower()))
            montage([os.path.join(d, 'x%d.png' % i) for i in range(9)], ['T %.3f' % x for x in xs],
                    '%d slots%s, %s' % (n, ' + Chain Loop' if loop else '', name), row, '9x1', '213x120+1+1')
            rows.append(row)
            print(row, flush=True)
    subprocess.run(['magick'] + rows + ['-append', os.path.join(out, 'styles.png')], check=True)


def rest_colours(out):
    """mean colour of each model at rest (harness means over covered pixels)"""
    L = ['bundle build/LoadEhr.bundle', 'size 320 180', 'instance'] + load(4) + ['realtime 1', 'advance 0 zero', 'hosttime 0']
    for i in range(4):
        L += ['set Transition %.6f' % (i / 3), 'frames 2', 'means 1']
    log = harness(L, os.path.join(out, 'rest.txt'))
    ref = []
    for l in log.splitlines():
        m = re.match(r'means frame\s+\d+ \| R\s+([\d.]+) G\s+([\d.]+) B\s+([\d.]+)', l)
        if m:
            ref.append(tuple(float(x) for x in m.groups()))
    return ref


def classify(rgb, ref, cnt):
    if cnt == 0:
        return '.'
    d = [sum((a - b) ** 2 for a, b in zip(rgb, r)) for r in ref]
    i = min(range(len(d)), key=d.__getitem__)
    # a mid-morph frame (between two references) shows as '~'
    s = sorted(d)
    return 'ABCD'[i] if s[0] < 0.2 * s[1] else '~'


def play(out):
    os.makedirs(out, exist_ok=True)
    ref = rest_colours(out)
    lines = ['rest colours (mean RGB of A..D): ' + ', '.join('%s (%.0f %.0f %.0f)' % ('ABCD'[i], *r) for i, r in enumerate(ref))]
    cases = [('Once On Trigger', 1, 0), ('Ping-Pong', 2, 0), ('Loop', 3, 0), ('Loop + Chain Loop', 3, 1)]
    for name, mode, loop in cases:
        L = ['bundle build/LoadEhr.bundle', 'size 320 180', 'instance'] + load(4) + [
            'set Transition Time 1', 'set Hold Time 0.5', 'set Chain Loop %d' % loop, 'set Transition Play %d' % mode, 'frames 1',
            'advance 0 zero', 'hosttime 0', 'means %d' % (14 * 10)]  # 14 s at 10 fps (frames are 1/60 s: every 6th kept below)
        # means renders 1/60 s frames: 140 frames = 2.33 s. Use 14 s worth: 840 frames, keep every 6th
        L[-1] = 'means 840'
        log = harness(L, os.path.join(out, 'play_%d%d.txt' % (mode, loop)))
        seq = []
        for l in log.splitlines():
            m = re.match(r'means frame\s+\d+ \| R\s+([\d.]+) G\s+([\d.]+) B\s+([\d.]+) \| max\(RGB\)\s+[\d.]+ \| (\d+) px', l)
            if m:
                seq.append(classify(tuple(float(x) for x in m.groups()[:3]), ref, int(m.group(4))))
        s = ''.join(seq[::6])  # one letter per 0.1 s
        lines.append('%-18s (Time 1 s, Hold 0.5 s; one letter = 0.1 s, ~ = mid-morph):' % name)
        for k in range(0, len(s), 50):
            lines.append('   %5.1f s  %s' % (k / 10, s[k:k + 50]))
    open(os.path.join(out, 'play.txt'), 'w').write('\n'.join(lines) + '\n')
    print('\n'.join(lines))


def scrub(out):
    os.makedirs(out, exist_ok=True)
    xs = [i / 12 for i in range(13)]
    L = ['bundle build/LoadEhrWow.bundle', 'size 640 360', 'instance'] + load(4) + ['set Liquid Wobble 0', 'realtime 1.5', 'advance 0 zero', 'hosttime 0']
    for s, name in enumerate(STYLES):
        L += ['set Transition Style %d' % s]
        for i, x in enumerate(xs):  # forward
            L += ['set Transition %.6f' % x, 'frames 3', 'shot %s/%s_f%02d.png' % (out, name.lower(), i)]
        for i, x in reversed(list(enumerate(xs))):  # backward over the same points
            L += ['set Transition %.6f' % x, 'frames 3', 'shot %s/%s_b%02d.png' % (out, name.lower(), i)]
        for i in range(len(xs)):
            L += ['compare %s/%s_f%02d.png %s/%s_b%02d.png' % (out, name.lower(), i, out, name.lower(), i)]
    log = harness(L, os.path.join(out, 'scrub.txt'))
    open(os.path.join(out, 'scrub.log'), 'w').write(log)
    res = {}
    for l in log.splitlines():
        m = re.match(r'compare \S+/(\w+)_f(\d+)\.png vs \S+: mean ([\d.]+) max (\d+) pixels>2 (\d+)', l)
        if m:
            res.setdefault(m.group(1), []).append((int(m.group(2)), float(m.group(3)), int(m.group(4)), int(m.group(5))))
    for k, v in res.items():
        print('%-8s forward vs backward at %d points: max diff %d, worst mean %.4f, pixels > 2: %d' % (k, len(v), max(x[2] for x in v), max(x[1] for x in v),
                                                                                                    sum(x[3] for x in v)))


def cost(out):
    os.makedirs(out, exist_ok=True)
    L = ['bundle build/LoadEhrWow.bundle', 'size 1920 1080', 'instance'] + load(4) + ['realtime 2', 'set Liquid Wobble 0.4']
    for s, name in enumerate(STYLES):
        L += ['set Transition Style %d' % s]
        for label, x in (('rest on B', 1 / 3), ('mid-hop B->C', 0.5)):
            L += ['set Transition %.6f' % x, 'frames 60', 'stats_reset', 'frames 120', 'stats %s %s' % (name, label)]
    log = harness(L, os.path.join(out, 'cost.txt'))
    open(os.path.join(out, 'cost.log'), 'w').write(log)
    for l in log.splitlines():
        if l.startswith('stats'):
            print(l)


def load_test(out):
    os.makedirs(out, exist_ok=True)
    heavy = [model_file(h) for h in HEAVY]
    L = ['bundle build/LoadEhrWow.bundle', 'size 1920 1080', 'instance', 'set Debug Log 1', 'text Model_A ' + MODELS[0], 'wait_loaded 60',
         'set Transition Play 3', 'set Chain Loop 1', 'set Transition Time 2', 'set Transition Style 0', 'realtime 1', 'stats_reset', 'mark']
    L += ['text Model_%s %s' % ('BCD'[i], heavy[i]) for i in range(3)]
    L += ['realtime 25', 'stats 3 heavy models loading while the chain plays (realtime)', 'stats_reset', 'realtime 8',
          'stats the 4-slot chain playing, all loaded (realtime)']
    log = harness(L, os.path.join(out, 'load.txt'))
    open(os.path.join(out, 'load.log'), 'w').write(log)
    for l in log.splitlines():
        if l.startswith(('stats', 'spikes')):
            print(l[:400])


if __name__ == '__main__':
    c, out = sys.argv[1], os.path.abspath(sys.argv[2])
    fns = {'styles': styles, 'play': play, 'scrub': scrub, 'cost': cost, 'load': load_test}
    for k in (fns if c == 'all' else [c]):
        print('==', k, flush=True)
        fns[k](os.path.join(out, k) if c == 'all' else out)
