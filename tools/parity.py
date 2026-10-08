#!/usr/bin/env python3
"""LoadEhr W1 parity: every non-laser MaskEhr look rendered in MaskEhr and in LoadEhr with the same model FILE, the same dials,
the same harness composites (arena and black), shots at 1 / 2 / 3 / 5 s. MaskEhr renders twice: its own run-to-run difference
is the noise floor (MaskEhr W26: 13 of 21 Wow looks are bit-reproducible, the rest bimodal / never repeat).
MaskEhr is READ ONLY: its bundles + harness are a reference build in a scratch folder (env MASKEHR_BUILD), never in its tree.

usage: tools/parity.py list
       tools/parity.py run <out> <side> [look ...]   side = L (LoadEhr), M1 / M2 (MaskEhr, two runs): <out>/<side>/<slug>/
       tools/parity.py report <out>                 compare L vs M1 and M2 vs M1 per shot -> <out>/parity.tsv + summary
       tools/parity.py strips <out> <dst>           side-by-side strips (MaskEhr top, LoadEhr bottom, diff x8) -> <dst>/<slug>.jpg
                                                    (a model / script naming a forbidden file stops the run: FORBIDDEN)
Looks = tools/w22_smoke.py (12), w24_trip.py (14), w25_looks.py (3), w26_wow.py (18), w28_review.py (6),
        + the name-based harness scripts in tools/scripts/ (each shot of each script)."""
import os, re, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.dont_write_bytecode = True
import w22_smoke as W
import w24_trip as T
import w25_looks as T25
import w26_wow as V
import w28_review as F

ROOT = W.ROOT
MEHR = os.environ.get('MASKEHR_BUILD', '/private/tmp/claude-501/-Users-amritrosell-Documents-claude-ffgl-LoadEhr/4b964771-8c73-47e3-8148-97956e9c25c7/scratchpad/mehr_build')
TIMES = (1, 2, 3, 5)
# MaskEhr's laser looks (intentionally gone in LoadEhr) and the looks that had lasers (rendered without them on both sides)
GONE = ['Laser Eyes', 'Laser Tunnel', 'Laser Scanner', 'Laser Scanner 2', 'Laser Fan 2', 'T Lasers']
STRIPPED = ['Infinity Corridor', 'Mirror Storm', 'T Corridor Box (MaskEhr T Lasers Box)']
SCRIPTS = ['smoke', 'w11_colours', 'w11_lights', 'w2', 'w23_air_fx', 'w3_anchor', 'w3_vol', 'w4_post', 'w5_surface', 'w6_shape', 'w7_copies',
           'w8_live', 'w9_face']


# Amrit (2026-10-08): "Carnival mask is absolutely forbidden to use!" Any recipe or script naming it stops the run.
FORBIDDEN = ('carnival',)


def check(slug, text):
    if any(f in text.lower() for f in FORBIDDEN):
        raise SystemExit('FORBIDDEN model in %s' % slug)


def looks():
    out = []
    for e in W.EXAMPLES:
        out.append(('w22_' + W.slug(e), T.preamble(e), {}, e.get('model')))
    for t in T.TRIPS:
        out.append(('w24_' + T.slug(t), T.preamble(T.recipe(t)), {}, T.recipe(t).get('model')))
    for t in T25.W25:
        out.append(('w25_' + T.slug(t), T.preamble(T.recipe(t)), {}, T.recipe(t).get('model')))
    for e in V.LOOKS:
        out.append(('w26_' + V.slug(e), V.preamble(e) + e.get('start', []), e.get('at', {}), e.get('model')))
    for f in F.FIXES:
        e = F.look(f['name'])
        out.append(('w28_' + V.slug(e), V.preamble(e) + e.get('start', []), e.get('at', {}), e.get('model')))
    return out


def schedule(pre, at, d):
    """shots at 1 / 2 / 3 / 5 s in both composites; a recipe's timed presses / slider moves at their own times"""
    L, t0 = list(pre), 0.0
    for s in sorted(set(TIMES) | set(at)):
        L.append('frames %d' % round((s - t0) * 60))
        L += at.get(s, [])
        t0 = s
        if s in TIMES:
            L += ['composite arena', 'shot %s/t%g_arena.png' % (d, s), 'composite black', 'shot %s/t%g_black.png' % (d, s)]
    return L


def to_maskehr(lines):
    """the same script for MaskEhr: its bundles (the scratch reference build) and its names for the renamed params"""
    out = []
    for l in lines:
        l = re.sub(r'%s/build/LoadEhr(\w*)\.bundle' % re.escape(ROOT), lambda m: '%s/MaskEhr%s.bundle' % (MEHR, m.group(1)), l)
        l = re.sub(r'^bundle build/LoadEhr(\w*)\.bundle', lambda m: 'bundle %s/MaskEhr%s.bundle' % (MEHR, m.group(1)), l)
        l = re.sub(r'\bModel_A\b', 'Model_File', l)
        l = re.sub(r'^(get|set) Model A\b', r'\1 Model File', l)
        l = re.sub(r'^get Status A$', 'get Status', l)
        l = re.sub(r'^(set|pulse) Reload A\b', r'\1 Reload', l)
        out.append(l)
    return out


# W2: the reference can be an earlier LoadEhr build instead of MaskEhr (PARITY_REF=<build dir>: its LoadEhr*.bundle +
# loadehr_harness, the same param names), and the L side a frozen copy of this build (PARITY_L=<build dir>)
REF = os.environ.get('PARITY_REF', '')
LBUILD = os.environ.get('PARITY_L', os.path.join(ROOT, 'build'))


def to_build(lines, d):
    out = []
    for l in lines:
        l = re.sub(r'%s/build/LoadEhr(\w*)\.bundle' % re.escape(ROOT), lambda m: '%s/LoadEhr%s.bundle' % (d, m.group(1)), l)
        l = re.sub(r'^bundle build/LoadEhr(\w*)\.bundle', lambda m: 'bundle %s/LoadEhr%s.bundle' % (d, m.group(1)), l)
        out.append(l)
    return out


# W2: Trip lives in the Wow bundles only (Spots in the classic ones): a look that dials them runs on that carrier on the L side
_INC = open(os.path.join(ROOT, 'src', 'ParamList.inc')).read()
TRIP = set(re.findall(r'X\( \w+, "([^"]+)", \w+, [^,]+, [^,]+, [^,]+, "Trip"', _INC)) | set(
    n for b in re.findall(r'MEHR_COLOR\( \w+, "([^"]+)", "Trip"', _INC) for n in (b + ' Color', b + ' Sat', b + ' Bri', b + ' Alpha'))
# W9: the Smoke Sim group went to the Wow bundles too
TRIP |= set(re.findall(r'X\( \w+, "([^"]+)", \w+, [^,]+, [^,]+, [^,]+, "Smoke Sim"', _INC)) | {'Smoke Color', 'Smoke Sat', 'Smoke Bri', 'Smoke Alpha'}


def to_carrier(lines):
    if not any(re.match(r'^set (.+?) -?[\d.]+$', l) and re.match(r'^set (.+?) -?[\d.]+$', l).group(1) in TRIP for l in lines):
        return lines
    return [re.sub(r'LoadEhr(FX)?\.bundle', lambda m: 'LoadEhrWow%s.bundle' % (m.group(1) or ''), l) if l.startswith('bundle') else l for l in lines]


def harness(side, lines, path):
    if side == 'L':
        if any('Face Track' in l for l in lines):  # W9: Face is carried by LoadEhr FX again
            lines = [l.replace('build/LoadEhrWowFX.bundle', 'build/LoadEhrFX.bundle') for l in lines]
        lines, exe = to_build(to_carrier(lines), LBUILD), os.path.join(LBUILD, 'loadehr_harness')
    elif REF:
        lines, exe = to_build(lines, REF), os.path.join(REF, 'loadehr_harness')
    else:
        if any('Face Track' in l for l in lines):  # Face: LoadEhr Wow FX carries it, MaskEhr FX did
            lines = [l.replace('build/LoadEhrWowFX.bundle', 'build/LoadEhrFX.bundle') for l in lines]
        lines, exe = to_maskehr(lines), os.path.join(MEHR, 'maskehr_harness')
    open(path, 'w').write('\n'.join(lines) + '\n')
    r = subprocess.run([exe, path], capture_output=True, text=True, cwd=ROOT)
    return r.stdout + r.stderr


def script_looks():
    """the name-based harness scripts: every `shot` (and `shot_rgba`) redirected into the side's folder"""
    out = []
    for name in SCRIPTS:
        lines = open(os.path.join(ROOT, 'tools', 'scripts', name + '.txt')).read().splitlines()
        out.append(('script_' + name, lines))
    return out


def run(out, side, names):
    W.ensure_input()
    for slug, pre, at, model in looks():
        check(slug, ' '.join(pre) + str(model))
    for slug, lines in script_looks():
        check(slug, ' '.join(lines))
    for slug, pre, at, _model in looks():
        if names and slug not in names:
            continue
        d = os.path.join(out, side, slug)
        os.makedirs(d, exist_ok=True)
        log = harness(side, schedule(pre, at, d), os.path.join(d, 'script.txt'))
        open(os.path.join(d, 'run.log'), 'w').write(log)
        print(side, slug, ' '.join(l for l in log.splitlines() if l.startswith('wait_loaded'))[:160], flush=True)
    for slug, lines in script_looks():
        if names and slug not in names:
            continue
        d = os.path.join(out, side, slug)
        os.makedirs(d, exist_ok=True)
        L, k = [], 0
        for l in lines:
            m = re.match(r'^(shot|shot_rgba) (\S+)$', l)
            if m:
                k += 1
                l = '%s %s/s%02d_%s' % (m.group(1), d, k, os.path.basename(m.group(2)))
            m = re.match(r'^(compare|delta) ', l)
            if m:
                continue  # the scripts' own compares point at MaskEhr-era files: parity compares instead
            L.append(l)
        log = harness(side, L, os.path.join(d, 'script.txt'))
        open(os.path.join(d, 'run.log'), 'w').write(log)
        print(side, slug, k, 'shots', flush=True)


def compare_all(out, pairs):
    lines = ['compare %s %s' % (a, b) for a, b in pairs]
    path = os.path.join(out, 'compare.txt')
    open(path, 'w').write('\n'.join(lines) + '\n')
    r = subprocess.run([os.path.join(ROOT, 'build', 'loadehr_harness'), path], capture_output=True, text=True, cwd=ROOT)
    res = {}
    for l in r.stdout.splitlines():
        m = re.match(r'compare (\S+) vs (\S+): mean ([\d.]+) max (\d+) pixels>2 (\d+) of (\d+)', l)
        if m:
            res[(m.group(1), m.group(2))] = (float(m.group(3)), int(m.group(4)), int(m.group(5)))
        elif 'CANNOT' in l:
            a, b = l.split()[1], l.split()[2]
            res[(a, b)] = None
    return res


def report(out):
    """per shot: every LoadEhr run (L, L2) against every MaskEhr run (M1, M2, M3 when present). identical = a LoadEhr run equals a
    MaskEhr run bit for bit; float noise = the nearest MaskEhr run is off by <= 2 / 255; noise = the nearest MaskEhr run is no
    further than MaskEhr's own runs are from each other (its run-to-run spread: bimodal / never-repeating looks); else DIFFERS"""
    sides = sorted(d for d in os.listdir(out) if os.path.isdir(os.path.join(out, d)) and re.match(r'^(L\d?|M\d)$', d))
    Ls, Ms = [d for d in sides if d.startswith('L')], [d for d in sides if d.startswith('M')]
    rows, pairs = [], []
    for slug in sorted(os.listdir(os.path.join(out, 'L'))):
        for f in sorted(x for x in os.listdir(os.path.join(out, 'L', slug)) if x.endswith('.png')):
            have = lambda d: os.path.exists(os.path.join(out, d, slug, f))
            ls, ms = [d for d in Ls if have(d)], [d for d in Ms if have(d)]
            P = lambda d: os.path.join(out, d, slug, f)
            for l in ls:
                for m in ms:
                    pairs.append((P(m), P(l)))
            for i in range(len(ms)):
                for j in range(i + 1, len(ms)):
                    pairs.append((P(ms[i]), P(ms[j])))
            rows.append((slug, f, ls, ms, P))
    res = compare_all(out, pairs)
    tsv = ['look\tshot\truns\tnearest L-M mean\tmax\tpx>2\tMaskEhr spread mean\tmax\tverdict']
    worst, order = {}, ['MISSING', 'DIFFERS', 'noise (within MaskEhr run-to-run)', 'float noise (max <= 2)', 'identical']
    for slug, f, ls, ms, P in rows:
        lm = [res.get((P(m), P(l))) for l in ls for m in ms]
        lm = [x for x in lm if x]
        mm = [res.get((P(ms[i]), P(ms[j]))) for i in range(len(ms)) for j in range(i + 1, len(ms))]
        mm = [x for x in mm if x]
        if not lm:
            v, near = 'MISSING', None
        else:
            near = min(lm, key=lambda x: (x[1], x[0]))
            spread = (max(x[0] for x in mm), max(x[1] for x in mm)) if mm else (0.0, 0)
            if near[1] == 0:
                v = 'identical'
            elif near[1] <= 2:
                v = 'float noise (max <= 2)'
            elif near[0] <= spread[0] * 1.25 + 0.02 and near[1] <= spread[1] * 1.25 + 2:
                v = 'noise (within MaskEhr run-to-run)'
            else:
                v = 'DIFFERS'
        sp = (max(x[0] for x in mm), max(x[1] for x in mm)) if mm else (0, 0)
        tsv.append('%s\t%s\t%s\t%s\t%.4f\t%d\t%s' % (slug, f, '+'.join(ls + ms), '%.4f\t%d\t%d' % near if near else '-\t-\t-', sp[0], sp[1], v))
        worst[slug] = min([worst.get(slug, 'identical'), v], key=order.index)
    open(os.path.join(out, 'parity.tsv'), 'w').write('\n'.join(tsv) + '\n')
    counts = {}
    for v in worst.values():
        counts[v] = counts.get(v, 0) + 1
    print('looks: %d, shots: %d (each look at its worst shot)' % (len(worst), len(rows)))
    for k in order[::-1]:
        if k in counts:
            print('  %-36s %d' % (k, counts[k]))
    for slug, v in sorted(worst.items()):
        if v in ('DIFFERS', 'MISSING'):
            print('  %s: %s' % (v, slug))


def strips(out, dst):
    os.makedirs(dst, exist_ok=True)
    for slug in sorted(os.listdir(os.path.join(out, 'L'))):
        dl, dm = os.path.join(out, 'L', slug), os.path.join(out, 'M1', slug)
        shots = sorted(f for f in os.listdir(dl) if f.endswith('_arena.png') or (slug.startswith('script_') and f.endswith('.png')))[:6]
        if not shots:
            continue
        tmp = os.path.join(out, 'strip_tmp')
        os.makedirs(tmp, exist_ok=True)
        top, bot, dif = [], [], []
        for f in shots:
            a, b = os.path.join(dm, f), os.path.join(dl, f)
            if not os.path.exists(a):
                continue
            d = os.path.join(tmp, 'd_' + f)
            subprocess.run(['magick', a, b, '-compose', 'difference', '-composite', '-evaluate', 'multiply', '8', '-alpha', 'off', d], check=True)
            top.append(a), bot.append(b), dif.append(d)
        args = ['montage']
        for row, label in ((top, 'MaskEhr'), (bot, 'LoadEhr'), (dif, 'diff x8')):
            for i, f in enumerate(row):
                args += ['-label', '%s %s' % (label, os.path.basename(f).replace('d_', '').replace('.png', '')), f]
        n = len(top)
        args += ['-tile', '%dx3' % n, '-geometry', '320x180+2+2', '-background', '#202020', '-fill', 'white', '-font', W.FONT, '-pointsize', '11',
                 '-title', slug, '-quality', '82', os.path.join(dst, slug + '.jpg')]
        subprocess.run(args, check=True)
        print(os.path.join(dst, slug + '.jpg'))


if __name__ == '__main__':
    c, a = sys.argv[1], sys.argv[2:]
    if c == 'list':
        for s, *_ in looks():
            print(s)
        for s, _ in script_looks():
            print(s)
        print('gone (lasers):', ', '.join(GONE))
        print('rendered without lasers:', ', '.join(STRIPPED))
    elif c == 'run':
        run(a[0], a[1], a[2:])
    elif c == 'report':
        report(a[0])
    elif c == 'strips':
        strips(a[0], a[1])
