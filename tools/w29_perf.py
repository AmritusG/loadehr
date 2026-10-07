#!/usr/bin/env python3
"""W29: the performance hunt's Arena instrument. Never triggers anything.
usage: w29_perf.py watch <tag>           background: tails the LoadEhr log; every instance that has written PERF lines for
                                           >= 8 s gets ONE 3 s system-wide Metal System Trace (record only: export later, its
                                           CPU would load the machine) -> verify/w29/<tag>/<instance>.trace; Ctrl-C / kill ends
       w29_perf.py export <tag>          exports every trace's GPU interval tables (slow, heavy CPU: run after the bench)
       w29_perf.py offline <tag> <layer:col> ...  each Arena clip's CURRENT values (REST) replayed in the harness (Wow / classic /
                                           FX bundle as in the clip), Debug Log on, 6 s realtime -> the PERF lines + harness stats;
                                           env LOADEHR_PERF_FINISH=1 = a glFinish at every pass boundary (per-pass GPU, serialised)
       w29_perf.py report <tag> [since]  per instance (clip): PERF windows -> fps median / slow end, cpu ms, per-pass cpu ms;
                                           + the trace: Arena frame period and GPU busy per frame (ATMOZ-018's method)
PERF line (LoadEhrPlugin::PerfTick): fps = 1 / the median interval between this instance's ProcessOpenGL calls; p95 = 1 / the
p95 interval; cpu = wall ms inside ProcessOpenGL. Debug Log must be on in the clip."""
import glob, os, re, subprocess, sys, time
import xml.etree.ElementTree as ET

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LOG = os.path.expanduser('~/Library/Logs/LoadEhr/LoadEhr.log')
PERF = re.compile(r'^(\S+ \S+) PERF (0x[0-9a-f]+) (\d+)x(\d+) \'([^\']*)\' n (\d+) fps ([\d.]+) p95 ([\d.]+) \| iv ms ([\d.]+)/([\d.]+) '
                  r'\| cpu ms ([\d.]+) p95 ([\d.]+) \| render ms ([\d.]+):(.*)$')


def out_dir(tag):
    d = os.path.join(ROOT, 'verify', 'w29', tag)
    os.makedirs(d, exist_ok=True)
    return d


def perf_lines(since=''):
    for line in open(LOG, errors='replace'):
        m = PERF.match(line.rstrip('\n'))
        if m and m.group(1) >= since:
            g = m.groups()
            passes = dict((k, float(v)) for k, v in re.findall(r' (\w+) ([\d.]+)', g[13]))
            yield dict(t=g[0], inst=g[1], w=int(g[2]), h=int(g[3]), model=g[4], n=int(g[5]), fps=float(g[6]), fps95=float(g[7]),
                       iv=float(g[8]), iv95=float(g[9]), cpu=float(g[10]), cpu95=float(g[11]), render=float(g[12]), passes=passes)


def watch(tag):
    d = out_dir(tag)
    first, done = {}, set()
    f = open(LOG, errors='replace')
    f.seek(0, 2)
    print('watching', LOG, '->', d, flush=True)
    while True:
        line = f.readline()
        if not line:
            time.sleep(0.2)
            continue
        m = PERF.match(line.rstrip('\n'))
        if not m:
            continue
        inst, now = m.group(2), time.time()
        first.setdefault(inst, now)
        if inst in done or now - first[inst] < 8:
            continue
        done.add(inst)
        stamp = time.strftime('%H%M%S')
        path = os.path.join(d, '%s_%s.trace' % (stamp, inst))
        print('%s trace %s (%s)' % (stamp, inst, m.group(5)), flush=True)
        subprocess.run(['xcrun', 'xctrace', 'record', '--template', 'Metal System Trace', '--all-processes', '--time-limit', '3s',
                        '--output', path, '--no-prompt'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        f.seek(0, 2)  # skip what was logged while recording (the windows a trace overlaps are marked by time in the report)


def export(tag):
    for t in sorted(glob.glob(os.path.join(out_dir(tag), '*.trace'))):
        for sc in ('metal-gpu-intervals', 'metal-gpu-submission-to-command-buffer-id'):
            x = t[:-6] + '_' + sc + '.xml'
            if os.path.exists(x):
                continue
            with open(x, 'w') as o:
                subprocess.run(['xcrun', 'xctrace', 'export', '--input', t, '--xpath',
                                '/trace-toc/run[@number="1"]/data/table[@schema="%s"]' % sc], stdout=o, stderr=subprocess.DEVNULL)
        print('exported', t)


# ---- offline replay of an Arena clip
def clip_script(layer, col, secs=6.0):
    import colorsys
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import w22_arena as A
    c = A.clip(layer, col)
    v = c['video']
    fx = [e for e in v.get('effects', []) if e['name'].startswith('LoadEhr')]
    name = fx[0]['name'] if fx else 'LoadEhr Wow' if 'Fire' in v['sourceparams'] else 'LoadEhr'
    bundle = {'LoadEhr': 'LoadEhr', 'LoadEhr FX': 'LoadEhrFX', 'LoadEhr Wow': 'LoadEhrWow', 'LoadEhr Wow FX': 'LoadEhrWowFX'}[name]
    sp = A.params_of(c)
    L = ['bundle %s/build/%s.bundle' % (ROOT, bundle), 'size 1920 1080'] + (['input bars'] if fx else []) + ['bpm 124', 'instance']
    tx = lambda k, val: 'text %s %s' % (k.replace(' ', '_'), str(val).replace('\n', '\\n'))
    if sp.get('Model B', {}).get('value'):
        L += [tx('Model B', sp['Model B']['value']), 'wait_loaded 240']
    for k in ('Model Source', 'Text', 'Font Name'):
        if k in sp:
            p = sp[k]
            L.append('set %s %d' % (k, p['options'].index(p['value'])) if p['valuetype'] == 'ParamChoice' else tx(k, p['value']))
    if sp.get('Model A', {}).get('value'):
        L.append(tx('Model A', sp['Model A']['value']))
    L.append('wait_loaded 240')
    for k, p in sp.items():
        t, val = p['valuetype'], p.get('value')
        if k in ('Status', 'Model A', 'Model B', 'Model Source', 'Text', 'Font Name', 'Debug Log', 'Font') or t == 'ParamEvent':
            continue
        if t == 'ParamRange':
            L.append('set %s %r' % (k, float(val)))
        elif t == 'ParamChoice':
            L.append('set %s %d' % (k, p['options'].index(val)))
        elif t == 'ParamBoolean':
            L.append('set %s %d' % (k, 1 if val else 0))
        elif t == 'ParamColor':
            r, g, b, a = (int(val[i:i + 2], 16) / 255.0 for i in (1, 3, 5, 7))
            h, sa, br = colorsys.rgb_to_hsv(r, g, b)
            L.append('set %s %r' % (k, h))
            L.append('setnext %s %r %r %r' % (k, sa, br, a))
        elif t in ('ParamFile', 'ParamString') and val:
            L.append(tx(k, val))
    L += ['realtime 1.5', 'set Debug Log 1', 'stats_reset', 'realtime %g' % secs, 'stats %s' % c['name']['value'].replace(' ', '_')]
    return c['name']['value'], L


def offline(tag, specs):
    d = out_dir(tag)
    for sp in specs:
        l, c = map(int, sp.split(':'))
        name, L = clip_script(l, c)
        path = os.path.join(d, 'L%dC%d.txt' % (l, c))
        open(path, 'w').write('\n'.join(L) + '\n')
        mark = time.strftime('%Y-%m-%d %H:%M:%S')
        r = subprocess.run([os.path.join(ROOT, 'build', 'loadehr_harness'), path], capture_output=True, text=True, cwd=ROOT)
        stats = [x for x in r.stdout.splitlines() if x.startswith('stats') or 'unknown' in x]
        ps = list(perf_lines(mark))[1:]
        keys = {}
        for x in ps:
            for k, v in x['passes'].items():
                keys.setdefault(k, []).append(v)
        top = sorted(((pct(v, 0.5), k) for k, v in keys.items()), reverse=True)
        print('L%dC%d %-22s render ms %.2f | %s' % (l, c, name, pct([x['render'] for x in ps], 0.5) if ps else -1, ' '.join('%s %.2f' % (k, v) for v, k in top)))
        for x in stats:
            print('    ' + x)


# ---- trace tables (AtMoz w39_mst.py's reading, without numpy)
def rows(path):
    ids = {}
    root = ET.parse(path).getroot()
    cols = [c.find('mnemonic').text for c in root.find('.//schema').findall('col')]
    for r in root.iter('row'):
        for el in r.iter():
            if el.get('id'):
                ids[el.get('id')] = el
        yield {n: (ids[el.get('ref')] if el.get('ref') else el) for n, el in zip(cols, list(r))}


def txt(el):
    return None if el is None or el.tag == 'sentinel' else (el.get('fmt') or el.text)


def union(iv):
    tot, cs, ce = 0.0, None, None
    for s, e in sorted(iv):
        if cs is None or s > ce:
            if cs is not None:
                tot += ce - cs
            cs, ce = s, e
        else:
            ce = max(ce, e)
    return tot + (ce - cs if cs is not None else 0.0)


def pct(v, q):
    v = sorted(v)
    return v[min(len(v) - 1, int(q * len(v)))] if v else float('nan')


def trace_summary(base):
    """Arena's GPU work: intervals attributed through the submission table; frames = Arena's own frame numbers"""
    gi, sb = base + '_metal-gpu-intervals.xml', base + '_metal-gpu-submission-to-command-buffer-id.xml'
    if not (os.path.exists(gi) and os.path.exists(sb)):
        return None
    sp = {}
    for r in rows(sb):
        k, p = r['gpu-submission-id'].text, txt(r['process'])
        if k and p:
            sp[k] = p
    arena, other, span = [], [], [1e30, 0]
    for r in rows(gi):
        if int(r['event-depth'].text) != 0:
            continue
        s = int(r['start'].text) / 1e6
        e = s + int(r['duration'].text) / 1e6
        p = txt(r['process']) or sp.get(r['gpu-submission-id'].text, '?')
        span = [min(span[0], s), max(span[1], e)]
        (arena if p and p.startswith('Arena') else other).append((s, e))
    T = span[1] - span[0]
    busy = union(arena)
    # Arena frames: its GPU work comes in bursts; a gap > 1.5 ms ends a frame's burst (AtMoz counted pass submissions; LoadEhr
    # has many passes per frame, so bursts are the frames)
    bursts, cur = [], None
    for s, e in sorted(arena):
        if cur is None or s - cur[1] > 1.5:
            if cur:
                bursts.append(cur)
            cur = [s, e, [(s, e)]]
        else:
            cur[1] = max(cur[1], e)
            cur[2].append((s, e))
    if cur:
        bursts.append(cur)
    st = [b[0] for b in bursts]
    dt = [b - a for a, b in zip(st, st[1:])]
    gb = [union(b[2]) for b in bursts]
    return dict(span=T, arenaBusy=busy / T * 100, otherBusy=union(other) / T * 100, frames=len(bursts),
                period=pct(dt, 0.5), period90=pct(dt, 0.9), gpu=pct(gb, 0.5), gpu90=pct(gb, 0.9))


def report(tag, since=''):
    d = out_dir(tag)
    per = {}
    for p in perf_lines(since):
        per.setdefault(p['inst'], []).append(p)
    traces = {os.path.basename(t).split('_')[1].split('.')[0]: t[:-6] for t in glob.glob(os.path.join(d, '*.trace'))}
    print('| instance | model | first | windows | fps med | fps slow (p95 iv) | cpu ms | render ms | top passes (cpu ms) | trace: period ms | GPU ms/frame (p90) | Arena GPU busy % |')
    print('|---|---|---|---|---|---|---|---|---|---|---|---|')
    for inst, L in sorted(per.items(), key=lambda kv: kv[1][0]['t']):
        L2 = L[2:] if len(L) > 4 else L  # the first windows hold the load / prewarm
        fps = pct([x['fps'] for x in L2], 0.5)
        slow = pct([x['fps95'] for x in L2], 0.5)
        cpu = pct([x['cpu'] for x in L2], 0.5)
        ren = pct([x['render'] for x in L2], 0.5)
        keys = {}
        for x in L2:
            for k, v in x['passes'].items():
                keys.setdefault(k, []).append(v)
        top = sorted(((pct(v, 0.5), k) for k, v in keys.items()), reverse=True)[:4]
        ts = trace_summary(traces[inst]) if inst in traces else None
        tr = '| %.2f | %.2f (%.2f) | %.0f' % (ts['period'], ts['gpu'], ts['gpu90'], ts['arenaBusy']) if ts else '| - | - | -'
        print('| %s | %s | %s | %d | %.1f | %.1f | %.2f | %.2f | %s %s |' % (inst, L[0]['model'], L[0]['t'][11:19], len(L), fps, slow, cpu, ren,
              ' '.join('%s %.2f' % (k, v) for v, k in top), tr))


if __name__ == '__main__':
    a = sys.argv[1:]
    if a[0] == 'watch':
        watch(a[1])
    elif a[0] == 'export':
        export(a[1])
    elif a[0] == 'offline':
        offline(a[1], a[2:])
    elif a[0] == 'report':
        report(a[1], a[2] if len(a) > 2 else '')
