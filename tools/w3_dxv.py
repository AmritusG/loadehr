#!/usr/bin/env python3
"""LoadEhr W3: the projector plays DXV files through DXVKit (third_party/dxvkit).

usage: tools/w3_dxv.py proof <out> [file ...]   every DXV variant: the projector's picture texture (harness, LOADEHR_VIDEO_DUMP) vs
                                                 DXVKit's own `dxvtool dump-rgba` of the same frames, crop included -> <out>/proof.md
       tools/w3_dxv.py alpha <out>              the DXT5 alpha and YG10 files over the background, Video Alpha Straight vs
                                                 Premultiplied -> <out>/alpha_<file>.png strips + the opaque-pixel check
       tools/w3_dxv.py longshow <out> [file]    Loop over a short DXV clip for 180 min (harness `advance`): the frame shown at 60 / 120 / 180 min
                                                 (Video Speed set to 0 at that instant, the picture dumped) vs the exact frame -> <out>/longshow.md
       tools/w3_dxv.py cost <out> [file ...]    real-time play (60 fps pacing), Debug Log VIDEO lines: decode / upload ms, late
                                                 frames per variant -> <out>/cost.md
dxvtool: built from the submodule into a scratch folder (DXVTOOL env, default $TMPDIR/dxvkit_tools_<submodule commit>/dxvtool: W8, a
DXVKit bump builds its own). LOADEHR_BUILD=<build dir> (W8): render with that build's bundles + harness (default build/)."""
import math, os, re, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.environ.get('LOADEHR_BUILD', os.path.join(ROOT, 'build'))
MASK = os.path.expanduser('~/Documents/Resolume Arena/MaskEhr Library/Skulls/Human Skull/scene.gltf')
LOG = os.path.expanduser('~/Library/Logs/LoadEhr/LoadEhr.log')
MOV = os.path.expanduser('~/Movies')
VEC = os.path.join(ROOT, 'third_party', 'dxvkit', 'test_vectors', 'streams', 'resolume')
FILES = [os.path.join(VEC, f) for f in sorted(os.listdir(VEC))] if os.path.isdir(VEC) else []
FILES += [MOV + '/Testfiles/Metric_01.mov',                                     # legacy DXDI DXT1
          MOV + '/KLV Set/06_THE GREAT SURRENDER_HQ - No Alpha No Audio.mov',   # DXT1
          MOV + '/black_snowflake_DXV Normal Quality With Alpha.mov',           # DXT5 4K alpha
          MOV + '/KLV Set/01_AWAKE & DISSOLVE_HQ - No Alpha No Audio.mov',      # YCG6
          MOV + '/Testfiles/YG10-DXV3.mov',                                     # YG10
          MOV + '/S06 Lone Star Halo 5s_Custom.mov',                            # YCG6 908x2276: padding on both axes
          MOV + '/S08 Intergalactic Void 5s.mov']                               # DXT5 908x2276
# W6: Amrit deleted ~/Movies/KLV Set ("Ran out of space, so deleted it."): files that are gone are skipped (and listed)
MISSING = [f for f in FILES if not os.path.exists(f)]
FILES = [f for f in FILES if os.path.exists(f)]
ALPHA_FILES = [MOV + '/Testfiles/DXT5-DXV3.mov', MOV + '/Testfiles/YG10-DXV3.mov']  # black_snowflake (DXT5 4K) is opaque in every frame
SCENE = [('Proj Enable', 1), ('Proj Pos X', 0.6), ('Proj Pos Y', 0.6), ('Proj Pos Z', 4), ('Proj Throw', 28), ('Proj Intensity', 40),
         ('Proj Beam', 1.2), ('Haze', 0.5), ('Key Intensity', 0.6), ('Debug Log', 1)]


def dxvtool():
    sha = subprocess.run(['git', '-C', os.path.join(ROOT, 'third_party', 'dxvkit'), 'rev-parse', '--short', 'HEAD'], capture_output=True, text=True).stdout.strip()
    t = os.environ.get('DXVTOOL', os.path.join(os.environ.get('TMPDIR', '/tmp'), 'dxvkit_tools_' + sha, 'dxvtool'))
    if not os.path.exists(t):
        b = os.path.dirname(t)
        subprocess.run(['cmake', '-S', os.path.join(ROOT, 'third_party', 'dxvkit'), '-B', b], check=True, capture_output=True)
        subprocess.run(['cmake', '--build', b, '-j', '--target', 'dxvtool'], check=True, capture_output=True)
    return t


def tag(path):
    return re.sub(r'[^A-Za-z0-9]+', '_', os.path.basename(path).rsplit('.', 1)[0])[:40]


def harness(lines, out, name, env=None):
    sp = os.path.join(out, name + '.txt')
    open(sp, 'w').write('\n'.join(lines) + '\n')
    return subprocess.run([os.path.join(BUILD, 'loadehr_harness'), sp], capture_output=True, text=True, cwd=ROOT,
                          env=dict(os.environ, **(env or {})))


def scene(path, extra=(), bundle='LoadEhrWow', size=(960, 540)):
    L = ['bundle %s/%s.bundle' % (BUILD, bundle), 'size %d %d' % size, 'instance', 'text Model_A ' + MASK, 'wait_loaded 60']
    L += ['set %s %g' % (k, v) for k, v in SCENE + list(extra)]
    return L + ['text Proj_File ' + path]


def info(path):
    r = subprocess.run([dxvtool(), 'info', path], capture_output=True, text=True).stdout
    m = re.search(r'(\d+)x(\d+)\s+frames (\d+)\s+([\d.]+) fps', r)
    v = re.search(r'variant (\S+(?: DXT\d)?)', r)
    return int(m.group(1)), int(m.group(2)), int(m.group(3)), float(m.group(4)), v.group(1)


def reference(path, frame, W, H, out, premul=False):
    ref = os.path.join(out, 'ref_%s_%d%s.rgba' % (tag(path), frame, '_pm' if premul else ''))
    if not os.path.exists(ref):
        subprocess.run([dxvtool(), 'dump-rgba', path, str(frame), ref] + (['--premultiply'] if premul else []), check=True, capture_output=True)
    d = open(ref, 'rb').read()
    assert len(d) == W * H * 4, (ref, len(d))
    return d


def shrink(ref, W, H, w, h):
    """the picture is drawn at w x h (<= 2048): a halving samples at the corner of 2 x 2 texels = their mean"""
    k = W // w
    assert k in (1, 2, 4) and H // h == k, (W, H, w, h)
    if k == 1:
        return [float(x) for x in ref], 1
    o = [0.0] * (w * h * 4)
    for y in range(h):
        for x in range(w):
            for c in range(4):
                s = 0
                for dy in range(k):
                    row = ((y * k + dy) * W + x * k) * 4 + c
                    for dx in range(k):
                        s += ref[row + dx * 4]
                o[(y * w + x) * 4 + c] = s / (k * k)
    return o, k


def diff(pic, w, h, ref, chans=3):
    """pic rows bottom-up (GL), ref rows top-first. mean / max over rgb, the histogram of |d|, and the last column / row (padding)"""
    tot, mx, n, hist, edge = 0.0, 0, 0, {}, 0
    for y in range(h):
        pr = (h - 1 - y) * w * 4
        rr = y * w * 4
        for x in range(w):
            for c in range(chans):
                d = abs(pic[pr + x * 4 + c] - ref[rr + x * 4 + c])
                tot += d
                n += 1
                q = int(round(d))
                hist[q] = hist.get(q, 0) + 1
                if q > mx:
                    mx = q
                if (x == w - 1 or y == h - 1) and q > edge:
                    edge = q
    return tot / n, mx, hist, edge


def proof(out, files):
    os.makedirs(out, exist_ok=True)
    rows = []
    if MISSING and not files:
        print('skipped (file gone): ' + ', '.join(os.path.basename(f) for f in MISSING))
    for path in files or FILES:
        W, H, nf, fps, var = info(path)
        dump = os.path.join(out, 'dump_' + tag(path)) + '/'
        os.makedirs(dump, exist_ok=True)
        for f in os.listdir(dump):
            os.remove(dump + f)
        for f in os.listdir(dump):
            os.remove(dump + f)
        # Video Alpha Premultiplied = the decoded colour as it is (no x alpha): compared with DXVKit's straight RGBA
        harness(scene(path, [('Video Alpha', 0), ('Video Speed', 0.5)]) + ['realtime 1.5'], out, 'p_' + tag(path), {'LOADEHR_VIDEO_DUMP': dump})
        got = sorted(((int(f.split('_')[0]), f) for f in os.listdir(dump) if f.endswith('.rgba')))
        pick = [got[0], got[len(got) // 2], got[-1]] if len(got) >= 3 else got
        for idx, fn in pick:
            w, h = map(int, re.search(r'_(\d+)x(\d+)\.rgba', fn).groups())
            pic = open(dump + fn, 'rb').read()
            ref, k = shrink(reference(path, idx, W, H, out), W, H, w, h)
            mean, mx, hist, edge = diff(pic, w, h, ref)
            top = ', '.join('%d: %.4f%%' % (q, 100.0 * c / sum(hist.values())) for q, c in sorted(hist.items())[:5])
            rows.append((os.path.basename(path), var, '%dx%d' % (W, H), '%dx%d%s' % (w, h, ' (1/%d)' % k if k > 1 else ''), idx, mean, mx, edge, top))
            print(rows[-1], flush=True)
    md = ['| file | variant | size | picture | frame | mean abs diff (rgb, 0..255) | max | max on the last row / column | |d| histogram (share of values) |',
          '|---|---|---|---|---|---|---|---|---|']
    for r in rows:
        md.append('| %s | %s | %s | %s | %d | %.4f | %d | %d | %s |' % r)
    open(os.path.join(out, 'proof.md'), 'w').write('\n'.join(md) + '\n')
    for path in files or FILES:
        d = os.path.join(out, 'dump_' + tag(path))
        for f in os.listdir(d):
            os.remove(os.path.join(d, f))
        os.rmdir(d)
    for f in os.listdir(out):
        if f.startswith('ref_'):
            os.remove(os.path.join(out, f))


def alpha(out):
    """Straight vs Premultiplied over the arena background, frame 0 (Video Speed 0); the opaque-pixel check on the picture:
    Straight x alpha leaves every alpha-255 pixel as it is"""
    os.makedirs(out, exist_ok=True)
    md = ['| file | variant | Straight vs Premultiplied (shot mean / max) | opaque pixels (a = 255) | their max diff Straight vs Premultiplied (picture) | transparent pixels (a = 0) | their mean rgb, Premultiplied | Straight picture vs dxvtool --premultiply (mean / max) |', '|---|---|---|---|---|---|---|---|']
    for path in ALPHA_FILES:
        W, H, nf, fps, var = info(path)
        t = tag(path)
        pics = {}
        for a, name in ((1, 'straight'), (0, 'premul')):
            dump = os.path.join(out, 'd_%s_%s/' % (t, name))
            os.makedirs(dump, exist_ok=True)
            harness(scene(path, [('Video Alpha', a), ('Video Speed', 0)], size=(1920, 1080)) + ['realtime 1.5', 'frames 4', 'composite arena',
                    'shot %s/%s_%s.png' % (out, t, name)], out, 'a_%s_%s' % (t, name), {'LOADEHR_VIDEO_DUMP': dump})
            fn = sorted(f for f in os.listdir(dump) if f.startswith('0_'))[-1]
            pics[name] = (open(dump + fn, 'rb').read(), fn)
            for f in os.listdir(dump):
                os.remove(dump + f)
            os.rmdir(dump)
        w, h = map(int, re.search(r'_(\d+)x(\d+)\.rgba', pics['premul'][1]).groups())
        ref = reference(path, 0, W, H, out)
        # the alpha itself: Straight (colour x alpha on the GPU) vs dxvtool --premultiply
        pm, _ = shrink(reference(path, 0, W, H, out, premul=True), W, H, w, h)
        smean, smax, _, _ = diff(pics['straight'][0], w, h, pm)
        k = W // w
        op = tr = 0
        opmax = 0
        trsum = 0.0
        for y in range(h):
            for x in range(w):
                ry, rx = y * k, x * k
                a = [ref[((ry + dy) * W + rx + dx) * 4 + 3] for dy in range(k) for dx in range(k)]
                p = ((h - 1 - y) * w + x) * 4
                if min(a) == 255:
                    op += 1
                    opmax = max(opmax, max(abs(pics['straight'][0][p + c] - pics['premul'][0][p + c]) for c in range(3)))
                elif max(a) == 0:
                    tr += 1
                    trsum += sum(pics['premul'][0][p:p + 3]) / 3
        r = subprocess.run([os.path.join(ROOT, 'build', 'loadehr_harness'), '/dev/stdin'], input='bundle %s/build/LoadEhrWow.bundle\nsize 64 64\ninstance\ncompare %s/%s_straight.png %s/%s_premul.png\n'
                           % (ROOT, out, t, out, t), capture_output=True, text=True, cwd=ROOT)
        cm = re.search(r'mean ([\d.]+) max (\d+)', r.stdout)
        subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', '%s/%s_straight.png' % (out, t), '-i', '%s/%s_premul.png' % (out, t), '-filter_complex',
                        '[0]scale=960:-1[a];[1]scale=960:-1[b];[a][b]hstack', '%s/alpha_%s.png' % (out, t)], check=True)
        for n in ('straight', 'premul'):
            os.remove('%s/%s_%s.png' % (out, t, n))
        md.append('| %s | %s | %s / %s | %d | %d | %d | %.2f | %.4f / %d |' % (os.path.basename(path), var, cm.group(1) if cm else '?', cm.group(2) if cm else '?', op, opmax, tr,
                  trsum / max(1, tr), smean, smax))
        print(md[-1], flush=True)
    for f in os.listdir(out):
        if f.startswith('ref_'):
            os.remove(os.path.join(out, f))
    open(os.path.join(out, 'alpha.md'), 'w').write('\n'.join(md) + '\n')


def cost(out, files):
    os.makedirs(out, exist_ok=True)
    md = ['| file | variant | size | decode ms / frame (worker) | upload ms (render thread, last / worst) | late frames | decode cache MB | plugin wall ms avg / p99 | GPU ms avg |',
          '|---|---|---|---|---|---|---|---|---|']
    for path in files or FILES:
        W, H, nf, fps, var = info(path)
        mark = os.path.getsize(LOG) if os.path.exists(LOG) else 0
        r = harness(scene(path, size=(1920, 1080)) + ['realtime 1', 'stats_reset', 'realtime 4', 'stats ' + tag(path)], out, 'c_' + tag(path))
        log = open(LOG, errors='replace').read()[mark:]
        vids = re.findall(r'VIDEO \S.* \| decoded (\d+) \(([\d.]+) ms each, worker\), late (\d+) \(\+(\d+)\), upload ([\d.]+) ms.*cache (\d+) MB', log)
        st = re.search(r'plugin wall ms avg\s+([\d.]+) p99\s+([\d.]+).*GPU ms avg\s+([\d.]+)', r.stdout)
        v = vids[-1] if vids else None
        ups = [float(x[4]) for x in vids]
        md.append('| %s | %s | %dx%d | %s | %s / %.2f | %s | %s | %s / %s | %s |' % (os.path.basename(path), var, W, H, v[1] if v else '?', v[4] if v else '?',
                  max(ups) if ups else 0, v[2] if v else '?', v[5] if v else '?', st.group(1) if st else '?', st.group(2) if st else '?', st.group(3) if st else '?'))
        print(md[-1], flush=True)
    open(os.path.join(out, 'cost.md'), 'w').write('\n'.join(md) + '\n')


def longshow(out, files):
    os.makedirs(out, exist_ok=True)
    path = (files or [MOV + '/S08 Intergalactic Void 5s.mov'])[0]
    W, H, nf, fps, var = info(path)
    length = nf / fps
    md = ['%s: %s %dx%d, %d frames at %g fps = %.4f s, Loop, Video Speed 1' % (os.path.basename(path), var, W, H, nf, fps, length), '',
          '| show time | frame shown | exact frame (floor(fmod(t, length) x fps)) |', '|---|---|---|']
    for minutes in (0.5, 60, 120, 180):
        T = minutes * 60
        dump = os.path.join(out, 'ls_%g/' % minutes)
        os.makedirs(dump, exist_ok=True)
        for f in os.listdir(dump):
            os.remove(dump + f)
        # the clip starts (first frame drawn), every clock back to 0, then T s of show time at 10 fps steps, then hold (speed 0)
        harness(scene(path, [('Video Speed', 0)], size=(320, 180)) + ['realtime 1', 'set Video Speed 1', 'advance 0 zero', 'advance %r' % T,
                'set Video Speed 0', 'realtime 1'], out, 'ls_%g' % minutes, {'LOADEHR_VIDEO_DUMP': dump})
        got = sorted(os.listdir(dump), key=lambda f: os.path.getmtime(dump + f))
        shown = int(got[-1].split('_')[0]) if got else -1
        exact = int(math.floor(math.fmod(T, length) * fps + 1e-6))
        md.append('| %g min | %d | %d |' % (minutes, shown, exact))
        print(md[-1], flush=True)
        for f in os.listdir(dump):
            os.remove(dump + f)
        os.rmdir(dump)
    open(os.path.join(out, 'longshow.md'), 'w').write('\n'.join(md) + '\n')


if __name__ == '__main__':
    cmd, out = sys.argv[1], sys.argv[2]
    if cmd == 'proof':
        proof(out, sys.argv[3:])
    elif cmd == 'alpha':
        alpha(out)
    elif cmd == 'longshow':
        longshow(out, sys.argv[3:])
    elif cmd == 'cost':
        cost(out, sys.argv[3:])
