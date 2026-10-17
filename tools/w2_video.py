#!/usr/bin/env python3
"""LoadEhr W2 Stage 5: the projector plays video files. Each test file (test_media/gen, made by `make`) thrown by the projector
onto a model in haze (LoadEhr Wow), played in real time (harness `realtime`, 60 fps pacing) with Debug Log on: the plugin's
VIDEO lines (decode ms per frame on the worker, upload ms on the render thread, frames shown late, decode cache MB) and the
harness stats (plugin wall ms, GPU ms). Shots at 1 s and 3 s (the frame must change: the video plays).

usage: tools/w2_video.py make                 the test files from test_media/faces_clip.mp4 (ffmpeg; HAP with glenc-cli)
       tools/w2_video.py run <out> [file ...]  -> <out>/<file>_t1.png / _t3.png, <out>/video.md
       tools/w2_video.py modes <out>           Loop / Ping-Pong / Once / Speed / Restart on one file (frame indices from shots)"""
import os, re, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GEN = os.path.join(ROOT, 'test_media', 'gen')
SRC = os.path.join(ROOT, 'test_media', 'faces_clip.mp4')
GLENC = os.path.expanduser('~/Documents/claude/native-builds/glenc-public/.build/release/glenc-cli')
MASK = os.path.expanduser('~/Documents/Resolume Arena/MaskEhr Library/Skulls/Human Skull/scene.gltf')
FILES = ['h264_1080.mp4', 'h264_4k.mp4', 'hevc_1080.mp4', 'hevc_4k.mp4', 'prores422_1080.mov', 'prores422_4k.mov', 'prores4444a_1080.mov',
         'hap_1080.mov', 'hap_4k.mov', 'hapalpha_1080.mov', 'hapq_1080.mov', 'hapq_4k.mov', 'hapqalpha_1080.mov']
LOG = os.path.expanduser('~/Library/Logs/LoadEhr/LoadEhr.log')
SCENE = [('Proj Enable', 1), ('Proj Pos X', 0.6), ('Proj Pos Y', 0.6), ('Proj Pos Z', 4), ('Proj Throw', 28), ('Proj Intensity', 40),
         ('Proj Beam', 1.2), ('Haze', 0.5), ('Key Intensity', 0.6), ('Debug Log', 1)]


def make():
    os.makedirs(GEN, exist_ok=True)
    ff = lambda *a: subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', SRC] + list(a), check=True, cwd=GEN)
    ff('-c', 'copy', 'h264_1080.mp4')
    ff('-vf', 'scale=3840:2160', '-c:v', 'libx264', '-crf', '18', '-pix_fmt', 'yuv420p', 'h264_4k.mp4')
    ff('-c:v', 'libx265', '-crf', '22', '-tag:v', 'hvc1', '-pix_fmt', 'yuv420p', 'hevc_1080.mp4')
    ff('-vf', 'scale=3840:2160', '-c:v', 'libx265', '-crf', '22', '-tag:v', 'hvc1', '-pix_fmt', 'yuv420p', 'hevc_4k.mp4')
    ff('-c:v', 'prores_ks', '-profile:v', '2', 'prores422_1080.mov')
    ff('-vf', 'scale=3840:2160', '-c:v', 'prores_ks', '-profile:v', '2', 'prores422_4k.mov')
    ff('-vf', "format=yuva444p10le,geq=lum='lum(X,Y)':cb='cb(X,Y)':cr='cr(X,Y)':a='if(lt(hypot(X-W/2,Y-H/2),H*0.45),1023,0)'",
       '-c:v', 'prores_ks', '-profile:v', '4', '-pix_fmt', 'yuva444p10le', 'prores4444a_1080.mov')
    g = lambda c, i, o, *a: subprocess.run([GLENC, '-c', c] + list(a) + [i, o], check=True, cwd=GEN, capture_output=True)
    g('hap1', 'h264_1080.mp4', 'hap_1080.mov')
    g('hap1', 'h264_4k.mp4', 'hap_4k.mov', '--chunks', '8')
    g('hapy', 'h264_1080.mp4', 'hapq_1080.mov')
    g('hapy', 'h264_4k.mp4', 'hapq_4k.mov', '--chunks', '4')
    g('hap5', 'prores4444a_1080.mov', 'hapalpha_1080.mov')
    g('hapm', 'prores4444a_1080.mov', 'hapqalpha_1080.mov')


def script(path, out, extra=(), shots=True, bundle='LoadEhrWow'):
    tag = os.path.basename(path).rsplit('.', 1)[0]
    L = ['bundle %s/build/%s.bundle' % (ROOT, bundle), 'size 1920 1080', 'instance', 'text Model_A ' + MASK, 'wait_loaded 60']
    L += ['set %s %g' % (k, v) for k, v in SCENE + list(extra)]
    L += ['text Proj_File ' + path, 'realtime 1']
    if shots:
        L += ['composite arena', 'shot %s/%s_t1.png' % (out, tag)]
    L += ['stats_reset', 'realtime 2', 'stats ' + tag]
    if shots:
        L += ['shot %s/%s_t3.png' % (out, tag), 'compare %s/%s_t1.png %s/%s_t3.png' % (out, tag, out, tag)]
    return L


def run(out, files):
    os.makedirs(out, exist_ok=True)
    rows = []
    for f in files or FILES:
        path = os.path.join(GEN, f)
        mark = os.path.getsize(LOG) if os.path.exists(LOG) else 0
        sp = os.path.join(out, f + '.txt')
        open(sp, 'w').write('\n'.join(script(path, out)) + '\n')
        r = subprocess.run([os.path.join(ROOT, 'build', 'loadehr_harness'), sp], capture_output=True, text=True, cwd=ROOT)
        log = open(LOG, errors='replace').read()[mark:]
        info = re.findall(r'PROJ video \S+: (.*)', log)
        vids = re.findall(r'VIDEO \S+ \| decoded (\d+) \(([\d.]+) ms each, worker\), late (\d+) \(\+(\d+)\), upload ([\d.]+) ms.*cache (\d+) MB', log)
        st = re.search(r'plugin wall ms avg\s+([\d.]+) p99\s+([\d.]+).*GPU ms avg\s+([\d.]+)', r.stdout)
        cmp = re.search(r'compare \S+ vs \S+: mean ([\d.]+)', r.stdout)
        last = vids[-1] if vids else None
        ups = [float(v[4]) for v in vids]
        rows.append((f, info[0] if info else '?', last, st.groups() if st else ('?', '?', '?'), max(ups) if ups else 0, cmp.group(1) if cmp else '?'))
        print(rows[-1], flush=True)
    md = ['| file | stream | decode ms / frame (worker) | upload ms (render thread, last / worst of the 1 s lines) | late frames (of ~180) | '
          'decode cache MB | plugin wall ms avg / p99 | GPU ms avg | frame 1 s vs 3 s (mean diff: it plays) |', '|---|---|---|---|---|---|---|---|---|']
    for f, info, v, st, up, cm in rows:
        md.append('| %s | %s | %s | %s / %.2f | %s | %s | %s / %s | %s | %s |' % (f, info, v[1] if v else '?', v[4] if v else '?', up, v[2] if v else '?',
                                                                         v[5] if v else '?', st[0], st[1], st[2], cm))
    open(os.path.join(out, 'video.md'), 'w').write('\n'.join(md) + '\n')
    print('\n'.join(md))


def modes(out):
    """frame positions read off a burnt-in frame number would need our own clip; instead the shots of each mode at fixed times are
    compared with each other: Loop at t and t + length are the same frame, Ping-Pong mirrors, Once holds the last frame"""
    os.makedirs(out, exist_ok=True)
    path = os.path.join(GEN, 'hap_1080.mov')
    L = ['bundle %s/build/LoadEhrWow.bundle' % ROOT, 'size 960 540', 'instance', 'text Model_A ' + MASK, 'wait_loaded 60']
    L += ['set %s %g' % (k, v) for k, v in SCENE] + ['set Haze 0', 'set Proj Beam 0', 'text Proj_File ' + path, 'realtime 0.5', 'composite black']
    for mode, name in ((0, 'loop'), (1, 'pingpong'), (2, 'once')):
        # the clock only runs in `frames` (Video Speed 1); while the decoder catches up it stands still (Video Speed 0, realtime)
        hold = lambda shot: ['set Video Speed 0', 'realtime 0.4', 'shot %s/%s_%s.png' % (out, name, shot), 'set Video Speed 1']
        L += ['set Video Loop %d' % mode, 'pulse Video Restart', 'set Video Speed 1', 'frames 6'] + hold('a')
        # +2 s, +4 s and +6 s more (6 s clip): loop: t 2.1, 6.1 (= 0.1), 12.1 (= 0.1); ping-pong: 2.1, 6.1 (= 5.9), 12.1 (= 0.1); once: 2.1, end, end
        L += ['frames 120'] + hold('b') + ['frames 240'] + hold('c') + ['frames 360'] + hold('d')
    open(os.path.join(out, 'modes.txt'), 'w').write('\n'.join(L) + '\n')
    r = subprocess.run([os.path.join(ROOT, 'build', 'loadehr_harness'), os.path.join(out, 'modes.txt')], capture_output=True, text=True, cwd=ROOT)
    print(r.stdout[-400:])


if __name__ == '__main__':
    c, a = sys.argv[1], sys.argv[2:]
    if c == 'make':
        make()
    elif c == 'run':
        run(a[0], a[1:])
    elif c == 'modes':
        modes(a[0])
