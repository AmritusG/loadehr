#!/usr/bin/env python3
"""LoadEhr W4: the projector video's scrubber (Video Loop Scrub, Video Position cue + playhead, Video Time, Video Speed < 0).

Test media: FRAME-CODED clips (frame n carries n in binary: 10 black / white blocks across the top quarter, bit 0 at the left), so
the picture itself says which frame is on screen. The projector's picture texture is dumped as each new frame is drawn
(LOADEHR_VIDEO_DUMP + LOADEHR_VIDEO_DUMP_SEQ: <seq>_<frame>_<w>x<h>.rgba, in the order shown) and its code read back.

usage: tools/w4_scrub.py make                       the coded clips -> test_media/gen/w4 (ffmpeg: ProRes master, DXV, H.264, HEVC;
                                                    glenc-cli: HAP, HAP Q)
       tools/w4_scrub.py sweep <out> [file ...]      Scrub 0 -> 1 -> 0 in 75 steps each way: every picture shown = round(p x (N - 1)),
                                                    in order, both directions -> <out>/sweep.md
       tools/w4_scrub.py cue <out> [file ...]        Loop at speed 1, a cue to 0.5: the next frame code is the cued frame, then it counts on
       tools/w4_scrub.py reverse <out> [file ...]    Video Speed -1 in Loop / Ping-Pong / Once: the codes run backwards (Once holds frame 0)
       tools/w4_scrub.py latency <out> [file ...]    Scrub latency per codec: jumps (random positions 0.4 s apart) and drags (a new
                                                    position every frame, 0 -> 1 and back over 2 s each) -> <out>/latency.md
       tools/w4_scrub.py longshow <out> [file]       Loop on a 5 s clip for 180 min at speed 1 and -1 (harness advance): the frame shown
                                                    at 60 / 120 / 180 min = the exact frame -> <out>/longshow.md
       tools/w4_scrub.py refuse <out> [file ...]     W6: Video Loop Scrub on every codec: DXV / HAP scrub (a Video Position change shows
                                                    that frame and the clock waits); anything else plays as Loop (a change cues, then it
                                                    counts on) and Video Time starts "Scrub: DXV / HAP only" -> <out>/refuse.md"""
import os, random, re, statistics, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GEN = os.path.join(ROOT, 'test_media', 'gen', 'w4')
GLENC = os.path.expanduser('~/Documents/claude/native-builds/glenc-public/.build/release/glenc-cli')
MASK = '/Users/amritrosell/Documents/Resolume Arena/MaskEhr Library/Skulls/Human Skull/scene.gltf'
LOG = os.path.expanduser('~/Library/Logs/LoadEhr/LoadEhr.log')
N = 150  # frames in every coded clip (5 s at 30 fps)
CODECS = ['dxv', 'hap', 'hapq', 'prores', 'h264', 'h264g30', 'hevc']
SIZES = {'1080': (1920, 1080), '4k': (3840, 2160)}
SCENE = [('Proj Enable', 1), ('Proj Pos X', 0.6), ('Proj Pos Y', 0.6), ('Proj Pos Z', 4), ('Proj Throw', 28), ('Proj Intensity', 40),
         ('Proj Beam', 1.2), ('Haze', 0.5), ('Key Intensity', 0.6), ('Debug Log', 1)]


def name(codec, size):
    return 'code_%s_%s.%s' % (codec, size, 'mp4' if codec.startswith(('h264', 'hevc')) else 'mov')


def files(args):
    return args or [os.path.join(GEN, name(c, s)) for s in SIZES for c in CODECS]


def make():
    os.makedirs(GEN, exist_ok=True)
    for size, (w, h) in SIZES.items():
        master = os.path.join(GEN, name('prores', size))
        code = "geq=lum='if(lt(Y\\,H/4)\\,255*mod(floor(N/pow(2\\,floor(X*10/W)))\\,2)\\,128+60*sin(X/40+N/7)*cos(Y/50))'"
        if not os.path.exists(master):
            subprocess.run(['ffmpeg', '-v', 'error', '-y', '-f', 'lavfi', '-i', 'color=c=gray:s=%dx%d:r=30:d=%g,format=gray,%s' % (w, h, N / 30, code),
                            '-c:v', 'prores_ks', '-profile:v', '2', '-pix_fmt', 'yuv422p10le', master], check=True)
        ff = lambda out, *a: os.path.exists(out) or subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', master] + list(a) + [out], check=True)
        ff(os.path.join(GEN, name('dxv', size)), '-c:v', 'dxv', '-pix_fmt', 'rgba')
        ff(os.path.join(GEN, name('h264', size)), '-c:v', 'libx264', '-crf', '18', '-pix_fmt', 'yuv420p')               # x264 default: 1 key frame
        ff(os.path.join(GEN, name('h264g30', size)), '-c:v', 'libx264', '-crf', '18', '-g', '30', '-pix_fmt', 'yuv420p')  # a key frame a second
        ff(os.path.join(GEN, name('hevc', size)), '-c:v', 'libx265', '-crf', '22', '-tag:v', 'hvc1', '-pix_fmt', 'yuv420p', '-x265-params', 'log-level=error')
        for c, g in (('hap', 'hap1'), ('hapq', 'hapy')):
            out = os.path.join(GEN, name(c, size))
            if not os.path.exists(out):
                subprocess.run([GLENC, '-c', g] + (['--chunks', '8'] if size == '4k' else []) + [master, out], check=True, capture_output=True)
    for f in sorted(os.listdir(GEN)):
        print(f, os.path.getsize(os.path.join(GEN, f)))


def read_code(path):
    """the frame number a dumped picture carries (rows bottom-up: the image's top quarter is the end of the buffer)"""
    m = re.search(r'(\d+)_(-?\d+)_(\d+)x(\d+)\.rgba$', path)
    w, h = int(m.group(3)), int(m.group(4))
    d = open(path, 'rb').read()
    row = h - 1 - h // 8
    bits, worst = 0, 0
    for b in range(10):
        x = int((b + 0.5) * w / 10)
        v = d[(row * w + x) * 4]
        worst = max(worst, min(v, 255 - v))
        bits |= (v > 128) << b
    return bits, worst, int(m.group(2))


def harness(lines, out, tag, dump=None):
    sp = os.path.join(out, tag + '.txt')
    open(sp, 'w').write('\n'.join(lines) + '\n')
    env = dict(os.environ)
    if dump:
        os.makedirs(dump, exist_ok=True)
        for f in os.listdir(dump):
            os.remove(os.path.join(dump, f))
        env.update(LOADEHR_VIDEO_DUMP=dump + '/', LOADEHR_VIDEO_DUMP_SEQ='1')
    mark = os.path.getsize(LOG) if os.path.exists(LOG) else 0
    r = subprocess.run([os.path.join(ROOT, 'build', 'loadehr_harness'), sp], capture_output=True, text=True, cwd=ROOT, env=env)
    log = open(LOG, errors='replace').read()[mark:]
    return r, log


def shown(dump):
    """the frame codes in the order they were drawn: (code, decoder index, worst bit margin)"""
    fs = sorted(f for f in os.listdir(dump) if f.endswith('.rgba'))
    return [read_code(os.path.join(dump, f)) for f in fs]


def scene(path, extra=(), size=(640, 360)):
    L = ['bundle %s/build/LoadEhrWow.bundle' % ROOT, 'size %d %d' % size, 'instance', 'text Model_A ' + MASK, 'wait_loaded 60']
    L += ['set %s %g' % (k, v) for k, v in SCENE + list(extra)]
    return L + ['text Proj_File ' + path, 'realtime 1.0']


def collapse(seq):
    r = []
    for v in seq:
        if not r or r[-1] != v:
            r.append(v)
    return r


def tag(path):
    return os.path.basename(path).rsplit('.', 1)[0]


def sweep(out, args):
    os.makedirs(out, exist_ok=True)
    rows = ['| file | steps | pictures shown | every round(p x (N - 1)) shown, in order | other pictures | worst bit margin (of 128) |', '|---|---|---|---|---|---|']
    steps = 75
    ps = [k / steps for k in range(steps + 1)] + [k / steps for k in range(steps - 1, -1, -1)]
    for path in files(args):
        dump = os.path.join(out, 'dump_' + tag(path))
        L = scene(path, [('Video Loop', 3), ('Video Position', 0)]) + ['realtime 0.5']
        for p in ps[1:]:
            L += ['set Video Position %.6f' % p, 'realtime 0.25']
        r, log = harness(L, out, 'sweep_' + tag(path), dump)
        got = shown(dump)
        want = collapse([int(p * (N - 1) + 0.5) for p in ps])
        codes = [g[0] for g in got]
        # every wanted frame shown, in order; a picture between two wanted ones lies on the way (a slow decode shows the nearest
        # decoded frame on the way from the 3rd render waiting; never a frame from the wrong side)
        k, extra, wrong = 0, 0, 0
        for c in codes:
            if k < len(want) and c == want[k]:
                k += 1
            elif 0 < k < len(want) and min(want[k - 1], want[k]) <= c <= max(want[k - 1], want[k]):
                extra += 1
            else:
                wrong += 1
        ok = k == len(want) and wrong == 0 and all(g[0] == g[2] for g in got)
        rows.append('| %s | %d | %d (want %d) | %s | %d in between, %d wrong | %d |' % (tag(path), len(ps), len(codes), len(want),
                    'yes' if ok else 'NO (%d of %d wanted shown)' % (k, len(want)), extra, wrong, max((g[1] for g in got), default=-1)))
        print(rows[-1], flush=True)
    open(os.path.join(out, 'sweep.md'), 'w').write('\n'.join(rows) + '\n')


def cue(out, args):
    os.makedirs(out, exist_ok=True)
    rows = ['| file | codes before the cue (last 3) | cue 0.5 -> frame | codes after it (first 8) | lands + counts on |', '|---|---|---|---|---|']
    for path in files(args):
        dump = os.path.join(out, 'dump_' + tag(path))
        # playing from 0: the cue arrives at ~1 s (frame ~30), 0.5 -> frame 75
        L = scene(path, [('Video Loop', 0), ('Video Speed', 0)]) + ['set Video Speed 1', 'pulse Video Restart', 'realtime 1.0',
                                                                     'set Video Position 0.5', 'realtime 0.5']
        r, log = harness(L, out, 'cue_' + tag(path), dump)
        codes = [g[0] for g in shown(dump)]
        k = next((i for i, c in enumerate(codes) if c >= 70), None)
        before, after = codes[:k][-3:] if k else codes[-3:], codes[k:k + 8] if k is not None else []
        target = int(0.5 * (N - 1) + 0.5)  # the plugin's lround (half away from zero; Python's round is half-even)
        ok = bool(after) and after[0] == target and all(b - a in (0, 1, 2) for a, b in zip(after, after[1:])) and after[-1] > target
        rows.append('| %s | %s | %d | %s | %s |' % (tag(path), before, target, after, 'yes' if ok else 'NO'))
        print(rows[-1], flush=True)
    open(os.path.join(out, 'cue.md'), 'w').write('\n'.join(rows) + '\n')


def reverse(out, args):
    os.makedirs(out, exist_ok=True)
    rows = ['| file | mode | codes shown (start .. end, collapsed) | runs backward | ends |', '|---|---|---|---|---|']
    for path in files(args):
        for mode, mname, secs in ((0, 'Loop', 7.0), (1, 'Ping-Pong', 7.0), (2, 'Once', 6.5)):
            dump = os.path.join(out, 'dump_%s_%s' % (tag(path), mname))
            L = scene(path, [('Video Loop', mode), ('Video Speed', 0)]) + ['set Video Speed -1', 'pulse Video Restart', 'realtime %g' % secs]
            r, log = harness(L, out, 'rev_%s_%d' % (tag(path), mode), dump)
            codes = collapse([g[0] for g in shown(dump)])
            steps = [b - a for a, b in zip(codes, codes[1:])]
            if mode == 0:  # backward, one wrap 0 -> 149
                ok = all(d < 0 or d > 100 for d in steps) and sum(d > 100 for d in steps) >= 1
                ends = 'wraps %d x' % sum(d > 100 for d in steps)
            elif mode == 1:  # Ping-Pong from 0 at speed -1: the clock wraps to 2 len, i.e. it starts on the way back from the end
                turns = sum(1 for a, b in zip(steps, steps[1:]) if (a < 0) != (b < 0))
                ok = all(d != 0 for d in steps) and turns >= 1
                ends = '%d turn(s)' % turns
            else:  # Once at a negative speed: the restart starts it from the end (frame 0 before it, at Speed 0), runs back, holds frame 0
                codes = codes[1:] if codes and codes[0] == 0 else codes
                steps = [b - a for a, b in zip(codes, codes[1:])]
                ok = all(d < 0 for d in steps) and codes[-1] == 0
                ends = 'holds %d' % codes[-1] if codes else '?'
            # frames skipped (a step of more than one frame, wraps / turns aside): where, and how many
            skips = [(i, abs(d) - 1) for i, d in enumerate(steps) if 1 < abs(d) <= 100]
            ends += '; skipped %d frames in %d steps%s' % (sum(k for _, k in skips), len(skips), (' (at picture %s)' % [codes[i] for i, _ in skips][:4]) if skips else '')
            rows.append('| %s | %s | %s .. %s (%d) | %s | %s |' % (tag(path), mname, codes[:4], codes[-4:], len(codes), 'yes' if ok else 'NO', ends))
            print(rows[-1], flush=True)
    open(os.path.join(out, 'reverse.md'), 'w').write('\n'.join(rows) + '\n')


def latency(out, args):
    os.makedirs(out, exist_ok=True)
    rows = ['| file | jumps: ms median / p95 / max | renders held median / max | drag fwd (120 steps, 1 per render): ms median / p95 | drag back | '
            'superseded targets (all phases) | jumps never shown |', '|---|---|---|---|---|---|---|']
    rnd = random.Random(4)
    jumps = [rnd.random() for _ in range(24)]
    for path in files(args):
        L = scene(path, [('Video Loop', 3), ('Video Position', 0)]) + ['realtime 0.5', 'stats_reset']
        for p in jumps:
            L += ['set Video Position %.6f' % p, 'realtime 0.4']
        L += ['set Video Position 0', 'realtime 1.1']
        for k in range(1, 121):
            L += ['set Video Position %.6f' % (k / 120), 'realtime 0.0166']
        L += ['realtime 1.1']
        for k in range(119, -1, -1):
            L += ['set Video Position %.6f' % (k / 120), 'realtime 0.0166']
        L += ['realtime 1.1']
        r, log = harness(L, out, 'lat_' + tag(path))
        open(os.path.join(out, 'lat_%s.log' % tag(path)), 'w').write(log)
        ev = [(int(f), float(m), int(h)) for f, m, h in re.findall(r'SCRUB \S+ frame (\d+): ([\d.]+) ms, held (\d+)', log)]
        sup = re.findall(r'SCRUB \S+: (\d+) targets superseded', log)
        # the jumps: matched in order (the load's own first frame and anything between them are not jumps); then the drags:
        # forward = the rising run after the last jump, back = the falling run after it
        jt = [int(p * (N - 1) + 0.5) for p in jumps]
        j, k = [], 0
        for i, e in enumerate(ev):
            if k < len(jt) and e[0] == jt[k]:
                j.append(e)
                k += 1
                last = i
        rest = ev[last + 1:] if j else []
        top = max(range(len(rest)), key=lambda i: rest[i][0]) if rest else 0
        fwd, back = [e for e in rest[:top + 1] if e[0] > 0], rest[top + 1:]
        def q(v, f):
            v = sorted(v)
            return v[min(len(v) - 1, int(f * len(v)))] if v else float('nan')
        jm = [m for _, m, _ in j]
        cell = lambda s: '%.1f / %.1f, held max %d (n %d)' % (q([m for _, m, _ in s], .5), q([m for _, m, _ in s], .95), max((h for *_, h in s), default=0), len(s)) if s else '-'
        missed = len(jt) - len(j)
        rows.append('| %s | %.1f / %.1f / %.1f | %d / %d | %s | %s | %s | %d |' % (tag(path), q(jm, .5), q(jm, .95), max(jm or [0]),
                    q([h for *_, h in j], .5), max((h for *_, h in j), default=0), cell(fwd), cell(back), sum(map(int, sup)), missed))
        print(rows[-1], flush=True)
    open(os.path.join(out, 'latency.md'), 'w').write('\n'.join(rows) + '\n')


def longshow(out, args):
    os.makedirs(out, exist_ok=True)
    path = args[0] if args else os.path.join(GEN, name('dxv', '1080'))
    fd = 1 / 30
    rows = ['| speed | minutes | frame shown (picture code) | exact frame | ok |', '|---|---|---|---|---|']
    for speed in (1, -1):
        for minutes in (60, 120, 180):
            dump = os.path.join(out, 'dump_%g_%g' % (speed, minutes))
            L = scene(path, [('Video Loop', 0), ('Video Speed', 0)]) + ['pulse Video Restart', 'realtime 0.3', 'set Video Speed %g' % speed,
                                                                         'advance %.6f zero' % (minutes * 60 + fd / 2), 'set Video Speed 0', 'realtime 1']
            r, log = harness(L, out, 'ls_%g_%g' % (speed, minutes), dump)
            got = shown(dump)
            # advance zero puts the clock at 0, then speed x (minutes + half a frame) on in 0.1 s steps; the clip is 5 s: a whole
            # number of loops + half a frame -> frame 0 forward, frame 149 backward (half a frame clear of either edge)
            t = (speed * (minutes * 60.0 + fd / 2)) % (N * fd)
            exact = int(t / fd + 1e-4) % N
            last = got[-1][0] if got else -1
            rows.append('| %g | %d | %d | %d | %s |' % (speed, minutes, last, exact, 'yes' if last == exact else 'NO'))
            print(rows[-1], flush=True)
    open(os.path.join(out, 'longshow.md'), 'w').write('\n'.join(rows) + '\n')


def refuse(out, args):
    os.makedirs(out, exist_ok=True)
    rows = ['| file | Video Time | codes after Video Position 0.5 (first 8, collapsed) | plays as | ok |', '|---|---|---|---|---|']
    for path in files(args):
        dump = os.path.join(out, 'dump_' + tag(path))
        L = scene(path, [('Video Loop', 3), ('Video Position', 0)]) + ['realtime 0.6', 'set Video Position 0.5', 'realtime 1.0', 'get Video Time']
        r, log = harness(L, out, 'refuse_' + tag(path), dump)
        vt = re.findall(r"get Video Time = '([^']*)'", r.stdout)
        vt = vt[-1] if vt else '?'
        codes = collapse([g[0] for g in shown(dump)])
        k = next((i for i, c in enumerate(codes) if c >= 70), None)
        after = codes[k:k + 8] if k is not None else []
        intra = any(c in tag(path) for c in ('dxv', 'hap'))
        scrubbed = bool(after) and after[-1] == 75  # Scrub: the frame stays on 75
        ok = (scrubbed and 'Scrub:' not in vt) if intra else (not scrubbed and len(after) > 3 and vt.startswith('Scrub: DXV / HAP only'))
        rows.append('| %s | %s | %s | %s | %s |' % (tag(path), vt, after, 'Scrub' if scrubbed else 'Loop', 'yes' if ok else 'NO'))
        print(rows[-1], flush=True)
    open(os.path.join(out, 'refuse.md'), 'w').write('\n'.join(rows) + '\n')


if __name__ == '__main__':
    c, a = sys.argv[1], sys.argv[2:]
    if c == 'make':
        make()
    else:
        {'sweep': sweep, 'cue': cue, 'reverse': reverse, 'latency': latency, 'longshow': longshow, 'refuse': refuse}[c](a[0], a[1:])
