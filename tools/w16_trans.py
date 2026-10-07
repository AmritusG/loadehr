#!/usr/bin/env python3
"""W16 transition onset measurement. usage (repo root):
  tools/w16_trans.py run <bundle> <out dir> <tag> [Name=value ...]   sweep all four styles, write <out>/<tag>.csv + strips
  tools/w16_trans.py report <out dir> <tag> [<tag> ...]              table + SVG/PNG plots of change per step

Each style is swept Transition 0 -> 1 in 100 steps, 2 frames per step (a 3.3 s scrub at 60 fps), our generated mask ->
DamagedHelmet, 640x360. Three passes per style: both models, A only, B only (LOADEHR_TRANS_SOLO). Per step:
  change   = mean |RGBA change| per channel vs the previous step (both models)
  covA/B   = pixels with alpha > 8 of A alone / B alone (mesh + dust points)
Strips (960x540) at Transition 0.05 / 0.15 / 0.5 / 0.85 / 0.95 from the both-models pass."""
import os, subprocess, sys

MODEL_A = os.path.abspath('test_models/gen/mask/mask.obj')  # LoadEhr: our generated mask (tools/gen_test_models.py)
MODEL_B = os.path.abspath('test_models/DamagedHelmet/DamagedHelmet.glb')
STYLES = ['Dissolve', 'Shatter', 'Dust', 'Slice']
STEPS, PER = 100, 2


def script(bundle, style, extra, size, prefix):
    lines = [f'bundle {bundle}', f'size {size}', 'instance', f'text Model_A {MODEL_A}', 'wait_loaded 60',
             f'text 5 {MODEL_B}', 'wait_loaded 60', f'set Transition Style {style}']
    lines += [f'set {k} {v}' for k, v in extra]
    lines += ['frames 30', f'sweep {STEPS} {PER} {prefix} Transition']
    return '\n'.join(lines) + '\n'


def sweep(bundle, style, extra, solo, size='640 360', prefix='-'):
    path = f'/tmp/w16_{os.getpid()}.txt'
    open(path, 'w').write(script(bundle, style, extra, size, prefix))
    env = dict(os.environ)
    if solo:
        env['LOADEHR_TRANS_SOLO'] = solo
    out = subprocess.run(['./build/loadehr_harness', path], capture_output=True, text=True, env=env).stdout
    os.remove(path)
    rows = [l.split() for l in out.splitlines() if l.startswith('sweep ')]
    return [(float(r[1]), float(r[3]), int(r[5])) for r in rows]


def run(bundle, outdir, tag, extra):
    os.makedirs(outdir, exist_ok=True)
    with open(f'{outdir}/{tag}.csv', 'w') as f:
        f.write('style,t,change,cover,covA,covB\n')
        for si, name in enumerate(STYLES):
            both = sweep(bundle, si, extra, None)
            a = sweep(bundle, si, extra, 'A')
            b = sweep(bundle, si, extra, 'B')
            for (t, ch, cov), (_, _, ca), (_, _, cb) in zip(both, a, b):
                f.write(f'{name},{t:.2f},{ch:.4f},{cov},{ca},{cb}\n')
            sweep(bundle, si, extra, None, '960 540', f'{outdir}/{tag}_strip_{name.lower()}')
            print(tag, name, 'done')


def load(outdir, tag):
    d = {}
    for line in open(f'{outdir}/{tag}.csv').read().splitlines()[1:]:
        s, t, ch, cov, ca, cb = line.split(',')
        d.setdefault(s, []).append((float(t), float(ch), int(cov), int(ca), int(cb)))
    return d


def svg(series, path, title):
    # series: [(label, colour, dash, [y per step])]; y normalised per series to its own peak
    W, H, L, B = 900, 420, 60, 50
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" font-family="Helvetica" font-size="13">',
           f'<rect width="{W}" height="{H}" fill="white"/>', f'<text x="{L}" y="22" font-size="15">{title}</text>']
    pw, ph = W - L - 200, H - B - 40
    for g in range(0, 11):
        x = L + pw * g / 10
        out.append(f'<line x1="{x}" y1="40" x2="{x}" y2="{40 + ph}" stroke="#ddd"/><text x="{x - 8}" y="{H - 28}">{g / 10:.1f}</text>')
    out.append(f'<text x="{L + pw / 2 - 40}" y="{H - 8}">Transition</text>')
    out.append(f'<text x="8" y="{40 + ph / 2}" transform="rotate(-90 14 {40 + ph / 2})">change per step (share of peak)</text>')
    for i, (label, col, dash, ys) in enumerate(series):
        peak = max(ys) or 1
        pts = ' '.join(f'{L + pw * (k + 1) / len(ys):.1f},{40 + ph * (1 - y / peak):.1f}' for k, y in enumerate(ys))
        out.append(f'<polyline points="{pts}" fill="none" stroke="{col}" stroke-width="2.2" stroke-dasharray="{dash}"/>')
        out.append(f'<line x1="{W - 190}" y1="{50 + i * 22}" x2="{W - 160}" y2="{50 + i * 22}" stroke="{col}" stroke-width="2.2" stroke-dasharray="{dash}"/>')
        out.append(f'<text x="{W - 152}" y="{55 + i * 22}">{label}</text>')
    out.append('</svg>')
    open(path, 'w').write('\n'.join(out))
    subprocess.run(['rsvg-convert', '-o', path[:-4] + '.png', path], check=False)


def smooth(ys, r=2):
    return [sum(ys[max(0, i - r):i + r + 1]) / len(ys[max(0, i - r):i + r + 1]) for i in range(len(ys))]


def report(outdir, tags):
    col = {'Dissolve': '#888', 'Slice': '#444', 'Shatter': '#d0402b', 'Dust': '#2b6fd0'}
    for tag in tags:
        d = load(outdir, tag)
        print(f'\n== {tag}: change per step (mean |d RGBA| per channel) ==')
        print('style     peak@t  peak   first10%  last10%  step1   step100  covB@0.01 covA@0.99  onset(0.01-0.05)/peak')
        for s in STYLES:
            ys = [r[1] for r in d[s]]
            pk = max(ys)
            print(f'{s:9} {d[s][ys.index(pk)][0]:.2f}  {pk:6.3f}  {sum(ys[:10]) / sum(ys):7.1%}  {sum(ys[-10:]) / sum(ys):7.1%}'
                  f'  {ys[0]:6.3f}  {ys[-1]:6.3f}  {d[s][0][4]:8d}  {d[s][98][3]:8d}   {max(ys[:5]) / pk:.2f}')
        svg([(s, col[s], '6 4' if s in ('Dissolve', 'Slice') else '', smooth([r[1] for r in d[s]])) for s in STYLES],
            f'{outdir}/{tag}_curves.svg', f'W16 {tag}: change per step, each style scaled to its own peak (5-step mean)')
    if len(tags) == 2:
        b, a = load(outdir, tags[0]), load(outdir, tags[1])
        print(f'\n== per-step table (change): {tags[0]} -> {tags[1]} ==')
        print('   t  ' + ' '.join(f'{s[:7] + " bef":>11} {"aft":>7}' for s in STYLES))
        for i in range(STEPS):
            if i < 10 or i % 5 == 4 or i >= 90:
                print(f'{b["Dissolve"][i][0]:.2f} ' + ' '.join(f'{b[s][i][1]:11.3f} {a[s][i][1]:7.3f}' for s in STYLES))


if __name__ == '__main__':
    if sys.argv[1] == 'run':
        run(os.path.abspath(sys.argv[2]), sys.argv[3], sys.argv[4], [kv.split('=') for kv in sys.argv[5:]])
    else:
        report(sys.argv[2], sys.argv[3:])
