#!/usr/bin/env python3
"""W23: smoke in the AIR. Uses the look-book recipes of tools/w22_smoke.py (EXAMPLES, the one source of truth).
usage: w23_air.py air <tag> [n ...]    5 s of each recipe with LOADEHR_SIM_STATS=1: the SIMAIR line nearest 5 s
                                       (fraction of smoke > 0.15 mask radii off the surface, plume p90 height)
                                       -> verify/w23/air_<tag>.tsv, and the 5 s frame over the textured background
                                       (composite arena) -> verify/w23/<tag>/<nn>_<slug>.png
       w23_air.py pair <tag> [n ...]   the 5 s frame flattened twice: as Arena reads a SOURCE below FFGL API 2.3
                                       (straight alpha) and at 2.3 (premultiplied) -> verify/w23/composite_<tag>_<slug>.png
       W23_EXTRA="Emit Offset=0.15;Smoke Ambient=1"  extra dials after the recipe (trials)
       w23_air.py sheet <tag>          verify/w23/<tag>/contact.png (all 12, over the background)"""
import os, re, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import w22_smoke as W

ROOT = W.ROOT
OUT = os.path.join(ROOT, 'verify', 'w23')
HARNESS = os.path.join(ROOT, 'build', 'loadehr_harness')

def base_script(e):
    """the W22 recipe script up to (and including) the Sim Reset press, no shots"""
    L = W.script(e, OUT).splitlines()
    i = L.index('pulse Sim Reset')
    # reproducible: the obstacle SDF (built off-thread when the sim first runs) lands before the reset, every clock
    # starts at 0 and the host clock is pinned (W16)
    extra = ['set %s %s' % tuple(kv.split('=')) for kv in os.environ.get('W23_EXTRA', '').split(';') if kv]  # trials
    return L[:i] + extra + ['realtime 1.5', 'advance 0 zero', 'hosttime 0'] + L[i:i + 1]

def harness(lines, path, env=None):
    open(path, 'w').write('\n'.join(lines) + '\n')
    r = subprocess.run([HARNESS, path], capture_output=True, text=True, cwd=ROOT, env=env)
    return r.stdout + r.stderr

def air(tag, args):
    d = os.path.join(OUT, tag)
    os.makedirs(d, exist_ok=True)
    W.ensure_input()
    env = dict(os.environ, LOADEHR_SIM_STATS='1', LOADEHR_LOG_STDERR='1')
    rows = []
    for e in W.by_n(args):
        png = os.path.join(d, W.slug(e) + '.png')
        out = harness(base_script(e) + ['composite arena', 'frames 300', 'shot ' + png], os.path.join(d, W.slug(e) + '.txt'), env)
        open(os.path.join(d, W.slug(e) + '.log'), 'w').write(out)
        m = re.findall(r'SIMAIR frame (\d+): air ([\d.]+) .*?p90 height (-?[\d.]+) radii, total ([\d.]+)', out)
        if not m:
            rows.append((e['n'], e['name'], '-', '-', '-'))
            print('%2d %-18s no SIMAIR line (noise engine or no sim)' % (e['n'], e['name']))
            continue
        f, a, h, t = min(m, key=lambda r: abs(int(r[0]) - 300))  # the frame nearest 5 s
        rows.append((e['n'], e['name'], a, h, t))
        print('%2d %-18s frame %s  air %s  p90 height %s radii  total %s' % (e['n'], e['name'], f, a, h, t))
    with open(os.path.join(OUT, 'air_%s.tsv' % tag), 'w') as f:
        f.write('n\tname\tair_fraction\tp90_height_radii\ttotal\n')
        for r in rows:
            f.write('\t'.join(str(x) for x in r) + '\n')

def pair(tag, args):
    W.ensure_input()
    for e in W.by_n(args):
        s, p = os.path.join(OUT, '_%s_straight.png' % W.slug(e)), os.path.join(OUT, '_%s_premult.png' % W.slug(e))
        harness(base_script(e) + ['frames 300', 'composite straight', 'shot ' + s, 'composite arena', 'shot ' + p],
                os.path.join(OUT, '_pair_%s.txt' % W.slug(e)))
        dst = os.path.join(OUT, 'composite_%s_%s.png' % (tag, W.slug(e)))
        subprocess.run(['montage', '-label', 'straight alpha (Arena, FFGL API 2.1 source)', s, '-label', 'premultiplied (FFGL API 2.3)', p,
                        '-tile', '2x1', '-geometry', '960x540+4+4', '-background', '#202020', '-fill', 'white', '-font', W.FONT,
                        '-pointsize', '22', '-title', '%s  (%s)' % (e['name'], tag), dst], check=True)
        for f in (s, p):
            os.remove(f)
        print(dst)

def sheet(tag):
    d = os.path.join(OUT, tag)
    frames = []
    for e in W.EXAMPLES:
        frames += ['-label', '%d %s' % (e['n'], e['name']), os.path.join(d, W.slug(e) + '.png')]
    subprocess.run(['montage'] + frames + ['-tile', '4x3', '-geometry', '640x360+4+4', '-background', '#202020',
                    '-fill', 'white', '-font', W.FONT, '-pointsize', '22', os.path.join(d, 'contact.png')], check=True)

if __name__ == '__main__':
    cmd = sys.argv[1] if len(sys.argv) > 1 else ''
    if cmd == 'air':
        air(sys.argv[2], sys.argv[3:])
    elif cmd == 'pair':
        pair(sys.argv[2], sys.argv[3:])
    elif cmd == 'sheet':
        sheet(sys.argv[2])
    else:
        print(__doc__)
