#!/usr/bin/env python3
"""LoadEhr W8: DXVKit 1.0.0 in the projector (Arena-exact DXV acceptance, HQ colour with chroma midpoint 127.5).

usage: tools/w8_dxv.py same <out> <ref build> [file ...]  the projector's picture (LOADEHR_VIDEO_DUMP) of this build vs a reference
                                                          build, frame by frame (frames both runs drew), byte for byte -> <out>/same.md.
                                                          Default files: every non-HQ DXV file of the W3 proof + the W2 HAP / Hap Q /
                                                          H.264 / HEVC / ProRes test files
       tools/w8_dxv.py forms <out>                        the 33 DXVKit Arena forms (~/Movies/Testfiles/dxvkit-w5-forms) through Proj
                                                          File, each after a playing control file: plays or refuses (as Arena's verdict
                                                          in arena_forms.tsv), the last picture kept on a refusal, no crash -> <out>/forms.md
The HQ proof is tools/w3_dxv.py proof (LOADEHR_BUILD=<build> for another build's pictures)."""
import os, re, subprocess, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import w3_dxv as w3

ROOT = w3.ROOT
GEN = os.path.join(ROOT, 'test_media', 'gen')
FORMS = os.path.expanduser('~/Movies/Testfiles/dxvkit-w5-forms')
TSV = os.path.join(ROOT, 'third_party', 'dxvkit', 'test_vectors', 'arena_forms.tsv')
CONTROL = os.path.join(w3.VEC, 'resolume-alley_dxt1_testsrc2_1920x1080.mov')  # a playing DXV file that is not one of the forms
OTHER = ['hap_1080.mov', 'hapalpha_1080.mov', 'hapq_1080.mov', 'hapqalpha_1080.mov', 'h264_1080.mp4', 'hevc_1080.mp4', 'prores422_1080.mov',
         'prores4444a_1080.mov']


def run(build, path, out, name, extra=()):
    dump = os.path.join(out, 'd_' + name) + '/'
    os.makedirs(dump, exist_ok=True)
    for f in os.listdir(dump):
        os.remove(dump + f)
    w3.BUILD = build
    r = w3.harness(w3.scene(path, [('Video Alpha', 0), ('Video Speed', 0.5)] + list(extra)) + ['realtime 1.5'], out, name, {'LOADEHR_VIDEO_DUMP': dump})
    pics = {}
    for f in os.listdir(dump):
        if f.endswith('.rgba'):
            pics[int(f.split('_')[0])] = open(dump + f, 'rb').read()
            os.remove(dump + f)
    os.rmdir(dump)
    return pics, r


def same(out, ref, files):
    os.makedirs(out, exist_ok=True)
    if not files:
        files = [f for f in w3.FILES if w3.info(f)[4] not in ('YCG6', 'YG10')] + [os.path.join(GEN, f) for f in OTHER]
    md = ['| file | frames compared | identical | max |diff| | mean |diff| |', '|---|---|---|---|---|']
    for path in files:
        t = w3.tag(path)
        a, _ = run(w3.BUILD0, path, out, 'new_' + t)
        b, _ = run(ref, path, out, 'ref_' + t)
        common = sorted(set(a) & set(b))
        ident = mx = 0
        tot = n = 0
        for i in common:
            if a[i] == b[i]:
                ident += 1
                n += len(a[i])
                continue
            d = [abs(x - y) for x, y in zip(a[i], b[i])]
            mx = max(mx, max(d))
            tot += sum(d)
            n += len(d)
        md.append('| %s | %d | %d | %d | %.4f |' % (os.path.basename(path), len(common), ident, mx, tot / max(1, n)))
        print(md[-1], flush=True)
    open(os.path.join(out, 'same.md'), 'w').write('\n'.join(md) + '\n')


def forms(out):
    os.makedirs(out, exist_ok=True)
    verdict = {}
    for line in open(TSV):
        if line.startswith('#') or not line.strip():
            continue
        col, f, v, form = line.rstrip('\n').split('\t')
        verdict[os.path.basename(f)] = (int(col), v, form)
    md = ['| col | form | Arena | LoadEhr | Video Time | new pictures | last picture kept | exit | agrees |', '|---|---|---|---|---|---|---|---|---|']
    agree = 0
    for name in sorted(verdict, key=lambda k: verdict[k][0]):
        col, arena, form = verdict[name]
        path = os.path.join(FORMS, name)
        dump = os.path.join(out, 'd_%02d/' % col)
        os.makedirs(dump, exist_ok=True)
        for f in os.listdir(dump):
            os.remove(dump + f)
        # a playing control first (its picture on screen), then the form: does it draw pictures, or is it refused and the control's
        # last picture stays? Shots before / after the switch
        L = w3.scene(CONTROL, [('Video Alpha', 0), ('Video Speed', 0), ('Haze', 0), ('Proj Beam', 0)]) + ['realtime 1', 'shot %s/a_%02d.png' % (out, col)]
        L += ['text Proj_File ' + path, 'realtime 1.5', 'get Video Time', 'shot %s/b_%02d.png' % (out, col)]
        w3.BUILD = w3.BUILD0
        r = w3.harness(L, out, 'f_%02d' % col, {'LOADEHR_VIDEO_DUMP': dump, 'LOADEHR_VIDEO_DUMP_SEQ': '1'})
        seq = sorted((int(f.split('_')[0]), f) for f in os.listdir(dump) if f.endswith('.rgba'))
        # the control (Speed 0) draws frame 0 once; every further picture came from the form
        fresh = len(seq) - 1
        for f in os.listdir(dump):
            os.remove(dump + f)
        os.rmdir(dump)
        vt = re.search(r"get Video Time = '(.*)'", r.stdout)
        vt = vt.group(1) if vt else '?'
        c = subprocess.run([os.path.join(w3.BUILD0, 'loadehr_harness'), '/dev/stdin'], input='bundle %s/LoadEhrWow.bundle\nsize 64 64\ninstance\ncompare %s/a_%02d.png %s/b_%02d.png\n'
                           % (w3.BUILD0, out, col, out, col), capture_output=True, text=True, cwd=ROOT)
        cm = re.search(r'mean ([\d.]+) max (\d+)', c.stdout)
        failed = vt.startswith('video failed')
        ours = 'refuse' if failed and fresh == 0 else 'play' if fresh > 0 and not failed else 'other'
        kept = '-' if ours == 'play' else ('yes (shot mean %s, max %s)' % (cm.group(1), cm.group(2)) if cm and float(cm.group(1)) < 0.5 else 'NO (%s)' % (cm.group(0) if cm else '?'))
        ok = r.returncode == 0 and (ours == 'play') == (arena == 'play') and ours != 'other' and not kept.startswith('NO')
        agree += ok
        md.append('| %d | %s | %s | %s | %s | %d | %s | %d | %s |' % (col, form, arena, ours, vt.replace('|', '/')[:90], fresh, kept, r.returncode, 'yes' if ok else 'NO'))
        print(md[-1], flush=True)
        for f in ('a_%02d.png' % col, 'b_%02d.png' % col):
            os.remove(os.path.join(out, f))
    md.append('')
    md.append('files %d | agree %d | disagree %d' % (len(verdict), agree, len(verdict) - agree))
    print(md[-1])
    open(os.path.join(out, 'forms.md'), 'w').write('\n'.join(md) + '\n')


w3.BUILD0 = w3.BUILD
if __name__ == '__main__':
    cmd, out = sys.argv[1], sys.argv[2]
    if cmd == 'same':
        same(out, sys.argv[3], sys.argv[4:])
    elif cmd == 'forms':
        forms(out)
