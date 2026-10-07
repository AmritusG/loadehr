#!/usr/bin/env python3
"""W28: Amrit's Wow review (fire / liquid / shard speeds, liquid plays on trigger, lava holds).
usage: w28_review.py timeline <look> <seconds> [harness line ...]   mean output colour every 0.25 s (640x360, reproducible)
       w28_review.py strip <look> <secs> <step> <dst.png> [harness line ...]   a shot every <step> s -> one montage
       w28_review.py speed <look> <secs> [harness line ...]   motion per frame, HEAD (env W28_HEAD = a HEAD worktree) vs this build
       w28_review.py lag <look> [harness line ...]            motion over 0.1 s minus the per-frame jitter, HEAD vs this build
       w28_review.py arena set|check <layer> <col> <look>     a FIXES look into an Arena clip over REST (never triggers)
Looks = tools/w26_wow.py LOOKS + FIXES below (the W28 fresh clips)."""
import os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import w26_wow as T

ROOT = T.ROOT
SP = os.environ.get('W28_TMP', '/tmp')
SKULL, MASK = T.SKULL, T.MASK

# the fresh W28 clips (soft, untriggered), next to the W26 originals. Their recipes: base W26 look + these dials (later win).
FIXES = [
    dict(name='Burning Letters 2', base='Burning Letters', dials=[('Fire Speed', 0.45), ('Burn Rate', 1.6), ('Burst Amount', 1.5), ('Fire Burst', 3)]),
    # renders: the plugin plays the morph (Transition Play Once). In Arena the clip's Transition is a timeline animation instead
    # (ARENA_ANIM: Play Once, Restart on re-trigger, the clip transport Play Once & Hold): a re-click while playing reaches no plugin
    dict(name='Liquid Morph 2', base='Liquid Morph', dials=[('Transition Play', 1), ('Transition Time', 6), ('Liquid Speed', 0.25)], at={}, times=(1, 2, 3, 4, 5, 6.5),
         arena=[('Transition Play', 0)]),
    dict(name='Liquid Paint 2', base='Liquid Paint', dials=[('Transition Play', 1), ('Transition Time', 7), ('Liquid Speed', 0.2)], at={}, times=(1, 2, 3, 4, 5, 7.5),
         arena=[('Transition Play', 0)]),
    dict(name='Bounce Shards 2', base='Bounce Shards', dials=[('Physics Speed', 0.35)], times=(0.5, 1.5, 3, 4.5, 7), at={7: []}),
    dict(name='Shard Wall 2', base='Shard Wall', dials=[('Physics Speed', 0.35)], times=(0.5, 1.5, 3, 4.5, 7), at={7: []}),
    dict(name='Lava Split 2', base='Lava Split', dials=[('Spin Y', 3)], start=[]),
]

# Arena-side, set with the arena MCP (REST has no animation endpoint here): parameter set_animation on the clip's Transition
# (phase_source timeline, playmode 'Play Once & Clear', play_mode_away 'Restart', duration = the seconds below, in 0, out 1)
# + clip set_transport (playmode 'Play Once & Hold', duration the same). clip_position is refused on FFGL source params.
ARENA_ANIM = {'Liquid Morph 2': 6, 'Liquid Paint 2': 7}


def look(name):
    for e in T.LOOKS:
        if e['name'] == name:
            return e
    f = next(x for x in FIXES if x['name'] == name)
    b = dict(look(f['base']))
    b.update({k: v for k, v in f.items() if k not in ('base', 'dials')})
    b['dials'] = list(b['dials']) + list(f['dials'])
    return b


def timeline(name, secs, extra):
    e = look(name)
    L = T.preamble(e, 640, 360) + list(extra) + ['means %d' % round(secs * 60)]
    out = T.harness(L, os.path.join(SP, 'w28_timeline.txt'))
    rows = [l for l in out.splitlines() if l.startswith('means frame')]
    for i, l in enumerate(rows):
        if i % 15 == 0:
            m = re.search(r'R +([\d.]+) G +([\d.]+) B +([\d.]+) .*\| +(\d+) px', l)
            print('%5.2f s  R %6.2f G %6.2f B %6.2f  px %s' % ((i + 1) / 60, *map(float, m.groups()[:3]), m.group(4)))


def strip(name, secs, step, dst, extra, w=480, h=270, root=None, t0=0.0):
    """a shot every <step> s up to <secs> -> one montage (dst)"""
    e = look(name)
    d = os.path.splitext(dst)[0]
    os.makedirs(d, exist_ok=True)
    L = T.preamble(e, w, h)
    if root:
        L = [l.replace(T.ROOT + '/build/', root + '/build/') for l in L]
    L += list(extra) + (['frames %d' % round(t0 * 60)] if t0 > 0 else [])
    n = int(round(secs / step))
    files = []
    for i in range(1, n + 1):
        L += ['frames %d' % round(step * 60), 'shot %s/f%03d.png' % (d, i)]
        files.append('%s/f%03d.png' % (d, i))
    out = T.harness(L, os.path.join(SP, 'w28_strip.txt'))
    open(d + '/run.log', 'w').write(out)
    import subprocess
    args = ['montage']
    for i, f in enumerate(files):
        args += ['-label', '%.2f s' % (t0 + (i + 1) * step), f]
    cols = min(n, 6)
    subprocess.run(args + ['-tile', '%dx' % cols, '-geometry', '%dx%d+2+2' % (w, h), '-background', '#202020', '-fill', 'white',
                           '-font', T.FONT, '-pointsize', '14', '-title', name, dst], check=True)
    print(dst)


HEAD = os.environ.get('W28_HEAD', os.path.join(SP, 'head'))  # a HEAD build (git worktree) for the "before" numbers


def speed(name, secs, extra, root=None, w=480, h=270, window=0.5):
    """motion: mean |per-frame change| of the output (0..255 per channel), averaged per window -> list of (t, value)"""
    e = look(name)
    L = T.preamble(e, w, h)
    if root:
        L = [l.replace(T.ROOT + '/build/', root + '/build/') for l in L]
    L += list(extra) + ['diffs %d' % round(secs * 60)]
    out = T.harness(L, os.path.join(SP, 'w28_speed.txt'))
    v = [float(x) for x in next(l for l in out.splitlines() if l.startswith('diffs:')).split()[1:]]
    n = round(window * 60)
    return [((i + n) / 60, sum(v[i:i + n]) / n) for i in range(0, len(v) - n + 1, n)]


def compare_speed(name, secs, extra, head_extra=None):
    a = speed(name, secs, head_extra if head_extra is not None else extra, HEAD)
    b = speed(name, secs, extra)
    print('%s: motion per frame (mean |change|), HEAD vs W28' % name)
    for (t, x), (_, y) in zip(a, b):
        print('  %5.2f s  %7.3f  %7.3f' % (t, x, y))
    ma, mb = sum(x for _, x in a) / len(a), sum(y for _, y in b) / len(b)
    print('  mean     %7.3f  %7.3f   (x%.2f)' % (ma, mb, mb / ma if ma else 0))



def lag(name, extra, root=None, starts=(3.0, 3.7, 4.4, 5.1, 5.8), k=6, w=480, h=270):
    """motion that is not per-frame jitter: mean |f(t + k frames) - f(t)| minus mean |f(t + 1) - f(t)| (0..255), averaged"""
    import subprocess
    e = look(name)
    d = os.path.join(SP, 'lag')
    os.makedirs(d, exist_ok=True)
    L = T.preamble(e, w, h)
    if root:
        L = [l.replace(T.ROOT + '/build/', root + '/build/') for l in L]
    L += list(extra)
    t = 0.0
    for i, s0 in enumerate(starts):
        L += ['frames %d' % round((s0 - t) * 60), 'shot %s/a%d.png' % (d, i), 'frames 1', 'shot %s/b%d.png' % (d, i),
              'frames %d' % (k - 1), 'shot %s/c%d.png' % (d, i)]
        t = s0 + k / 60
    T.harness(L, os.path.join(SP, 'w28_lag.txt'))

    def mae(x, y):
        r = subprocess.run(['magick', 'compare', '-metric', 'MAE', x, y, 'null:'], capture_output=True, text=True)
        return float(r.stderr.split()[0]) / 257.0
    one = [mae('%s/a%d.png' % (d, i), '%s/b%d.png' % (d, i)) for i in range(len(starts))]
    kk = [mae('%s/a%d.png' % (d, i), '%s/c%d.png' % (d, i)) for i in range(len(starts))]
    return sum(kk) / len(kk) - sum(one) / len(one), sum(one) / len(one)


if __name__ == '__main__':
    c, a = sys.argv[1], sys.argv[2:]
    if c == 'timeline':
        timeline(a[0], float(a[1]), a[2:])
    elif c == 'strip':  # strip <look> <secs> <step> <dst.png> [harness line ...]
        strip(a[0], float(a[1]), float(a[2]), a[3], a[4:])
    elif c == 'arena':
        import w26_arena as AR
        e = look(' '.join(a[3:]))
        (AR.set_look if a[0] == 'set' else AR.check)(int(a[1]), int(a[2]), e)
    elif c == 'lag':  # lag <look> [harness line ...]: HEAD vs this build
        a, b = lag(a[0], a[1:], HEAD), lag(a[0], a[1:])
        print('lag motion (0.1 s minus 1 frame): HEAD %.3f (floor %.3f)  W28 %.3f (floor %.3f)  x%.2f' % (a[0], a[1], b[0], b[1], b[0] / a[0]))
    elif c == 'speed':  # speed <look> <secs> [harness line ...]: HEAD vs this build
        compare_speed(a[0], float(a[1]), a[2:])
