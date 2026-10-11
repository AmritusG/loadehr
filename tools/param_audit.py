#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""LoadEhr W10: the parameter audit. Does every control reach pixels, and what does it need first?

usage (repo root, after a Release build):
  tools/param_audit.py static                 Stage 1: carried / read / consumers per id and bundle -> docs/audit/static.json + .md
  tools/param_audit.py sweep <bundle> [group|name ...]   Stage 2: the visible-effect sweep in the harness -> docs/audit/sweep_<bundle>.json
  tools/param_audit.py sheets                 Stage 2.4: Copies after LOD Bias, scene G at min / max -> docs/audit/copies_*.png
  tools/param_audit.py map                    Stage 3: docs/param_hierarchy.md from static.json + the sweeps + tools/param_audit_gates.json
  tools/param_audit.py canuse                 W11: every option / param a bundle offers that it cannot load or use -> docs/audit/canuse.md
  tools/param_audit.py all                    static, the four sweeps, map, canuse (sheets: on request only)

THE VISIBILITY RULE (one rule for every param): render the scene with the param at its default (twice: the run-to-run noise)
and at each test value, same clocks (hosttime pinned + `advance 0 zero` before the frames), and compare the raw premultiplied
host frames (480x270 RGBA, `ref_cmp`). A value is VISIBLE when >= 130 pixels (0.1 % of the frame) change by more than 8 / 255
in some channel AND that count is > 3x the default-vs-default noise count of the same scene.
SCENES: S = Human Skull in slot A (SciFi Helmet in slot B), still; M = S + Spin Y 30 + Wobble Amount 1 + Pos X 0.4 (the model
moves); G = M + every gate the code puts in front of that param opened (tools/param_audit_gates.json `recipe`).
CLASSES: VISIBLE ALWAYS (S) / VISIBLE ONLY WHEN MOVING (M, not S) / VISIBLE ONLY WHEN <gates> (G only) / NO VISIBLE EFFECT.
A param read by the code (static WIRED) with no visible effect even in G is BROKEN unless it is non-visual by design
(`nonvisual` in the gates file: status lines, reloads of an unchanged file, debug, hidden stores).
"""
import json
import os
import re
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'src')
AUD = os.path.join(ROOT, 'docs', 'audit')
HARNESS = os.path.join(ROOT, 'build', 'loadehr_harness')
GATES = os.path.join(ROOT, 'tools', 'param_audit_gates.json')
LIB = '/Users/amritrosell/Documents/Resolume Arena/MaskEhr Library'  # MaskEhr's folder: two files by path, nothing listed
SKULL = LIB + '/Skulls/Human Skull/scene.gltf'
HELMET = LIB + '/Helmets/SciFi Helmet/SciFiHelmet.gltf'
BUNDLES = {'LoadEhr': (0, 0), 'LoadEhrFX': (1, 0), 'LoadEhrWow': (0, 1), 'LoadEhrWowFX': (1, 1)}
REGION = {'0': 0, 'MEHR_WOW': 2, 'MEHR_CLASSIC': 4, '2': 2, '4': 4}
W, H = 480, 270
VIS_PX = 130          # 0.1 % of 480 x 270
VIS_NOISE = 3.0       # x the default-vs-default noise of the same scene
VIS_SHIFT = 0.5       # chaotic scenes: the frame's mean change of one channel, in 1/255 (see visible())
SETTLE = 90           # frames after the clocks are zeroed, unless the gates file asks for more
MOTION = ['set Spin Y 30', 'set Wobble Amount 1', 'set Pos X 0.4']
LOADS = {'Text', 'Logo', 'Extrude'}  # a change rebuilds the model: wait for its swap
STATELESS = {'Transform', 'Camera', 'Material', 'Surface', 'Parts', 'Lighting', 'Light A', 'Light B', 'Spots', 'Post', 'Floor',
             'Silhouette', 'FX', 'Output', 'Input', 'Wireframe', 'Deform', 'Volume', 'Cut', 'Surface Look', 'Build', 'Lines', 'Motion'}


# ---------------------------------------------------------------- the list itself (the C preprocessor expands the macros)
def defs(fx):
    with tempfile.NamedTemporaryFile('w', suffix='.c', delete=False) as f:
        f.write('#define X( id, name, type, mn, mx, def, group, opts, flags ) @@P id ~~ name ~~ type ~~ mn ~~ mx ~~ def ~~ group ~~ '
                'opts ~~ flags ~~ MEHR_REGION @@E\n#include "ParamList.inc"\n')
        c = f.name
    out = subprocess.run(['clang', '-E', '-P', '-DLOADEHR_FX=%d' % fx, '-I', SRC, c], capture_output=True, text=True).stdout
    os.unlink(c)
    rows = []
    for rec in out.replace('\n', ' ').split('@@E'):
        if '@@P' not in rec:
            continue
        f = [x.strip() for x in rec.split('@@P')[1].split('~~')]
        s = lambda x: ''.join(re.findall(r'"([^"]*)"', x))
        num = lambda x: float(x.rstrip('f')) if re.match(r'^-?[\d.]+f?$', x) else 0.0
        rows.append(dict(id=f[0], name=s(f[1]), type=f[2], min=num(f[3]), max=num(f[4]), default=num(f[5]), group=s(f[6]),
                         opts=s(f[7]).split('|') if s(f[7]) else [], hidden='HIDDEN' in f[8], fxonly='FXONLY' in f[8],
                         region=REGION[f[9]]))
    return rows


def all_defs():
    fx = defs(1)
    plain = {d['id'] for d in defs(0)}
    for d in fx:
        d['fx_only'] = d['id'] not in plain or d['fxonly']  # W11: FXONLY = an id every build has, carried by the FX bundles only
    return fx


def opt_name(o):
    """an option element's shown name: W7 '~' = retired, W11 '{wow}' / '{classic}' = one bundle family only"""
    return o.lstrip('~').split('{')[0]


def opt_here(o, bundle):
    """is this option element listed in that bundle (the plugin's OptionHere)"""
    if o.startswith('~'):
        return False
    wow = BUNDLES[bundle][1]
    if o.endswith('{wow}'):
        return bool(wow)
    if o.endswith('{classic}'):
        return not wow
    return True


def carried(d, bundle):
    fx, wow = BUNDLES[bundle]
    if d['fx_only'] and not fx:
        return False
    return d['region'] == 0 or d['region'] == (2 if wow else 4)


LINE = re.compile(r'^\s+(\d+) \| (.*?)\s*\| (.*?)\s*\| (.*?)\s*\| type\s+(\d+) \| range (\S+?)\.\.(\S+) \| default (\S+) \| '
                  r'elements (\d+) \| (visible|hidden)(?: \| options (.*))?$')


def harness(lines, tag='h'):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False, prefix='audit_%s_' % tag) as f:
        f.write('\n'.join(lines) + '\n')
        p = f.name
    r = subprocess.run([HARNESS, p], cwd=ROOT, capture_output=True, text=True)
    os.unlink(p)
    return r.stdout + r.stderr


def dump(bundle):
    rows = []
    for line in harness(['bundle build/%s.bundle' % bundle, 'instance', 'params']).splitlines():
        m = LINE.match(line)
        if m:
            rows.append(dict(i=int(m.group(1)), group=m.group(2), name=m.group(3), type=int(m.group(5)), lo=float(m.group(6)),
                             hi=float(m.group(7))))
    return rows


# ---------------------------------------------------------------- Stage 1: who reads each id
def sources():
    out = {}
    for fn in sorted(os.listdir(SRC)):
        if fn.endswith(('.cpp', '.h', '.mm')) and fn not in ('Params.h',):
            out[fn] = open(os.path.join(SRC, fn), errors='replace').read().splitlines()
    return out


FUNC = re.compile(r'^\s{0,4}(?:static\s+|inline\s+|virtual\s+)*[\w:<>,\*& ]+?\b([A-Za-z_]\w*)\s*\([^;]*$')


def func_of(lines, i):
    """the function a line sits in: the nearest earlier line at indentation <= 1 tab that looks like a signature"""
    for j in range(i, max(-1, i - 1500), -1):
        l = lines[j]
        if l.startswith(('\t\t', '        ')) or not l.strip() or l.strip().startswith(('//', '#', '}', '{')):
            continue
        m = FUNC.match(l)
        if m and m.group(1) not in ('if', 'for', 'while', 'switch', 'return', 'sizeof', 'X', 'MEHR_COLOR'):
            return m.group(1)
    return '?'


def static():
    D = all_defs()
    ids = [d['id'] for d in D]
    pos = {d['id']: k for k, d in enumerate(D)}
    src = sources()
    refs = {i: [] for i in ids}
    tok = re.compile(r'\bP_[A-Z0-9_]+\b')

    def add(i, fn, ln, why=''):
        if i in refs:
            refs[i].append((fn, ln, why))

    for fn, lines in src.items():
        for ln, l in enumerate(lines):
            code = l.split('//')[0]
            for m in tok.finditer(code):
                i = m.group(0)
                if i not in refs:
                    continue
                add(i, fn, ln)
                after = code[m.end():m.end() + 12]
                if i.endswith('_H'):  # a picker quartet: Colour( .. X_H .. ) reads H S V, Alpha( .. X_H .. ) reads A (same line or the
                    near = ' '.join(x.split('//')[0] for x in lines[ln:ln + 6])  # next few, through a variable / F( L, .. ))
                    lab = [i] + (['P_LB_' + i[5:]] if i.startswith('P_LA_') and 'F(' in code.replace(' ', '') else [])
                    for q in lab:
                        if re.search(r'\b(Colour|ColourDrift)\s*\(', near):
                            for s in ('_S', '_V'):
                                add(q[:-2] + s, fn, ln, 'Colour(%s)' % q)
                        if re.search(r'\bAlpha\s*\(', near):
                            add(q[:-2] + '_A', fn, ln, 'Alpha(%s)' % q)
                if re.match(r'\s*\+\s*[a-z]\b', after) and i.endswith('_X'):  # P_ROT_X + k
                    for s in ('_Y', '_Z'):
                        add(i[:-2] + s, fn, ln, '%s + k' % i)
                if i.startswith('P_LA_') and 'F(' in code.replace(' ', ''):  # lights A / B share code: F( L, P_LA_x )
                    add('P_LB_' + i[5:], fn, ln, 'F( L, %s )' % i)
            # ranges: index >= P_A && index <= P_B
            for a, b in re.findall(r'>=\s*(P_[A-Z0-9_]+)\s*&&\s*\w+\s*<=\s*(P_[A-Z0-9_]+)', code):
                if a in pos and b in pos:
                    for k in range(pos[a] + 1, pos[b]):
                        add(ids[k], fn, ln, 'range %s..%s' % (a, b))
            for f in re.findall(r'PartParam\s*\([^,]+,\s*(PF_\w+)', code):
                field = {'PF_VISIBLE': 'VISIBLE', 'PF_HUE': 'HUE', 'PF_GLOW': 'GLOW', 'PF_SKIN': 'SKIN', 'PF_SURFACE': 'SURFACE'}.get(f)
                for i in ids:
                    if re.match(r'P_PART\d_%s$' % field, i or ''):
                        add(i, fn, ln, 'PartParam(%s)' % f)
            for f in re.findall(r'Slot(?:Param|OfParam)\s*\([^,]+,\s*(SF_\w+)', code):
                pre = {'SF_FILE': 'P_MODEL_', 'SF_STATUS': 'P_STATUS_', 'SF_RELOAD': 'P_RELOAD_'}[f]
                for s in 'ABCD':
                    add(pre + s, fn, ln, 'SlotParam(%s)' % f)
    # a read = a reference outside the host's param store / logging / param-info setup (SetFloatId, SetParamInfo calls)
    store_funcs = {'GetFloatId', 'GetTextId', 'GetDisplayId', 'InitParams', 'SetupParams'}
    dumps = {b: dump(b) for b in BUNDLES}
    out = []
    for d in D:
        i = d['id']
        cons, reads = {}, 0
        for fn, ln, why in refs[i]:
            fu = func_of(src[fn], ln)
            line = src[fn][ln]
            if fu in store_funcs or 'SetParamInfo' in line or 'SetOptionParamInfo' in line or 'SetParamVisibility' in line \
               or 'SetParamDisplayName' in line or 'SetFileParamInfo' in line or 'SetParamElements' in line:
                continue
            reads += 1
            cons.setdefault('%s:%s' % (fn, fu), []).append(ln + 1)
        row = dict(d, consumers={k: v[:6] for k, v in cons.items()}, reads=reads, bundles={})
        for b, rows in dumps.items():
            car = carried(d, b)
            ci = [x['id'] for x in D if carried(x, b)]
            idx = ci.index(i) if car else None
            st = 'NOT CARRIED'
            if car:
                hr = rows[idx] if idx < len(rows) else None
                ok = hr is not None and hr['name'] == d['name'] and hr['group'] == d['group']
                st = 'CARRIED' if ok else 'MISROUTED (host %s = %r)' % (idx, hr and hr['name'])
            row['bundles'][b] = dict(index=idx, status=st)
        row['static'] = 'WIRED' if reads else 'NOT READ'
        out.append(row)
    os.makedirs(AUD, exist_ok=True)
    json.dump(out, open(os.path.join(AUD, 'static.json'), 'w'), indent=1)
    counts = {b: len(r) for b, r in dumps.items()}
    md = ['# W10 static wiring audit (generated by tools/param_audit.py static)', '',
          'Host index <-> id: computed from ParamList.inc regions (= `Map()`), checked name + group against the harness `params` dump.',
          'Bundles carry: ' + ', '.join('%s %d' % kv for kv in counts.items()), '',
          '| id | name | group | LoadEhr | FX | Wow | Wow FX | consumers (file:function lines) | static |', '|---|---|---|---|---|---|---|---|---|']
    for r in out:
        cell = lambda b: ('%d' % r['bundles'][b]['index']) if r['bundles'][b]['status'] == 'CARRIED' else \
            ('—' if r['bundles'][b]['status'] == 'NOT CARRIED' else '**%s**' % r['bundles'][b]['status'])
        cons = '; '.join('%s %s' % (k, ','.join(map(str, v[:3]))) for k, v in list(r['consumers'].items())[:4])
        md.append('| %s | %s | %s | %s | %s | %s | %s | %s | %s |' % (r['id'], r['name'], r['group'], cell('LoadEhr'), cell('LoadEhrFX'),
                                                                   cell('LoadEhrWow'), cell('LoadEhrWowFX'), cons, r['static']))
    open(os.path.join(AUD, 'static.md'), 'w').write('\n'.join(md) + '\n')
    bad = [r for r in out if r['static'] != 'WIRED' or any(v['status'].startswith('MIS') for v in r['bundles'].values())]
    print('static: %d ids, carried %s; not read / misrouted: %d' % (len(out), counts, len(bad)))
    for r in bad:
        print('  %-22s %-18s %s %s' % (r['id'], r['name'], r['static'], [v['status'] for v in r['bundles'].values() if v['status'].startswith('MIS')]))


# ---------------------------------------------------------------- Stage 2: the sweep
def gates():
    return json.load(open(GATES)) if os.path.exists(GATES) else {}


def fmt(v):
    return ('%.4f' % v).rstrip('0').rstrip('.') if v != int(v) else str(int(v))


def test_values(d, g, bundle=None):
    """each test = a list of harness commands (the param's action); an option value the bundle does not list is not tested (W11)"""
    t = g.get('test')
    if d['type'] == 'OPT' and bundle and d['opts'] and isinstance(t, list):
        t = [v for v in t if not isinstance(v, (int, float)) or int(v) >= len(d['opts']) or opt_here(d['opts'][int(v)], bundle)]
    if d['type'] == 'EVENT' or t == 'pulse':
        return [['pulse %s' % d['name']]]
    if isinstance(t, list) and any(isinstance(x, str) and x.startswith('then:') for x in t):  # one sequence: a press, then more
        first = [setcmd(d, t[0])] if t[0] != 'pulse' else [['pulse %s' % d['name']]]
        return [first[0] + [x[5:].strip() for x in t[1:]]]
    if isinstance(t, list):  # values, or a list of harness commands per test (a sequence: set, then more)
        return [v if isinstance(v, list) else setcmd(d, v) for v in t if v != 'ignored']
    tp = d['type']
    if tp in ('TEXT', 'FILE', 'BUFFER'):
        return []
    if tp == 'OPT':
        n = len(d['opts'])
        vals = [k for k in range(n) if k != round(d['default']) and (opt_here(d['opts'][k], bundle) if bundle else not d['opts'][k].startswith('~'))]
        return [setcmd(d, v) for v in (vals[:3] if n > 30 else vals)]  # Font: a few families
    if tp == 'BOOL':
        return [setcmd(d, 0 if d['default'] > 0.5 else 1)]
    return [setcmd(d, v) for v in (d['min'], d['max']) if abs(v - d['default']) > 1e-6]


def label(cmds):
    c = cmds[0] if cmds[0] != 'mark' else cmds[1]
    if c.startswith('pulse'):
        return 'pulse' + ('+' if len(cmds) > 1 else '')
    return c.split()[-1][-24:].replace('|', '/')


LOADING = ('Model A', 'Model B', 'Model C', 'Model D', 'Text', 'Logo File', 'Font File', 'Font Name')


def setcmd(d, v):
    if isinstance(v, str):  # a text / file value
        c = ['text %s %s' % (d['name'].replace(' ', '_'), v)]
        return ['mark'] + c + ['wait_loaded 8'] if d['name'] in LOADING else c + ['frames 30']
    c = ['set %s %s' % (d['name'], fmt(v))]
    if d['group'] in LOADS or d['name'] in ('Model Source', 'Max Texture Size', 'Shatter Mode', 'Shard Count'):
        c = ['mark'] + c + ['wait_loaded 6']
    return c


FRESH = [False]  # every run in a fresh instance (sweep --fresh)
DIRTY = [False]  # the last run in this instance loaded a model (see block)
PRE = 30  # frames of the scene before the param acts (trails, smoke, a shatter in the recipe get going)


def block(d, g, bundle, out_dir):
    """S, M, G; per scene the baseline twice (the 2nd = the noise), then every test. A run: clocks pinned and zeroed, the scene
    (+ gates), PRE frames, the param's action, the settle frames, compare with the baseline's last frame."""
    tests = test_values(d, g, bundle)
    settle = max(SETTLE, int(g.get('settle_frames') or SETTLE))
    settle += (3 - settle) % 30  # the param acts for N + 0.05 s and the shot lands 0.1 beat after a beat (PRE = 30 frames = a beat at the
    L = []                       # harness's 120 bpm): beat kicks are near their peak, and no dial has run a whole number of seconds (a crawl of
                                 # 1.0 vs 8.0 periods, or a sweep of exactly 1, looks like none)
    recipe = clean_recipe((g.get('recipe_in') or {}).get(bundle) or g.get('recipe') or [])
    for sc, cmds in (('S', []), ('M', MOTION), ('G', MOTION + recipe)):
        base = '%s_%s_base' % (d['id'], sc)

        def run(action, tag):
            # a run that loaded another model (or text) leaves it in slot A: the restore loads the skull back, so wait for that swap
            head = ['mark', 'restore_state', 'wait_loaded 8'] if DIRTY[0] else ['restore_state']
            if FRESH[0]:  # stateful looks (dust, camera cuts, sims): every run in a new instance, nothing carried over
                head = ['delete', 'instance', 'text Model_A ' + SKULL, 'wait_loaded 60', 'mark', 'text Model_B ' + HELMET, 'wait_slots 1 60']
            DIRTY[0] = any('wait_loaded' in c for c in cmds + action)
            # one frame at the defaults first: it ends a running trail (its stamp count restarts), so every run starts alike
            pin = lambda cs: sum(([c, 'hosttime 100', 'advance 0 zero'] if c.startswith('wait_') else [c] for c in cs), [])  # a load waits
            rt = ['realtime %g' % g['realtime']] if g.get('realtime') else []  # a worker that answers in wall time (face tracking)
            r = head + ['frames 1', 'hosttime 100', 'advance 0 zero'] + pin(cmds) + rt + ['frames %d' % PRE] + pin(action) + rt + ['frames %d' % settle]
            return r + (['ref_save %s' % base] if tag is None else ['ref_cmp %s %s' % (tag, base)])
        L += run([], None)
        L += run([], '%s|%s|noise' % (d['id'], sc))
        for k, t in enumerate(tests):
            L += run(t, '%s|%s|%s' % (d['id'], sc, label(t)))
    return L, tests


NAMES = None


def clean_recipe(rec):
    """the agents' recipes as harness lines: one-token `text` keys, model loads waited for, no bundle / input switches"""
    global NAMES
    if NAMES is None:
        NAMES = sorted((d['name'] for d in all_defs()), key=len, reverse=True)
    out = []
    for c in rec:
        w = c.split()
        if w[0] == 'bundle' or (w[0] == 'input' and w[-1] == 'bars'):  # the FX prologue already has the bars; `input <png>` stays
            continue
        if w[0] == 'text':
            rest = c[5:].strip()
            n = next((n for n in NAMES if rest.startswith(n + ' ') or rest.startswith(n.replace(' ', '_') + ' ')), None)
            if n:
                val = rest[len(n):].strip()
                c = 'text %s %s' % (n.replace(' ', '_'), val)
                if n in LOADING:
                    out += ['mark', c, 'wait_loaded 8']
                    continue
        out.append(c)
    return out


def prologue(bundle):
    fx = BUNDLES[bundle][0]
    return (['bundle build/%s.bundle' % bundle, 'size %d %d' % (W, H)] + (['input bars'] if fx else []) +
            ['instance', 'text Model_A ' + SKULL, 'wait_loaded 60', 'mark', 'text Model_B ' + HELMET, 'wait_slots 1 60', 'save_state'])


CMP = re.compile(r'^ref_cmp (\S+) mean ([\d.]+) max (\d+) changed (\d+) cover (\d+) (\d+) of (\d+)(?: shift (\S+) (\S+) (\S+) (\S+))?')


def sweep(bundle, only=(), fresh=False):
    st = {r['id']: r for r in json.load(open(os.path.join(AUD, 'static.json')))}
    G = gates()
    todo = [r for r in st.values() if r['bundles'][bundle]['status'] == 'CARRIED']
    if only:
        todo = [r for r in todo if r['group'] in only or r['name'] in only]
    # jobs: a fresh instance per param, except stateless groups (one instance per group)
    jobs, groups = [], {}
    for r in todo:
        g = G.get(r['name'], {})
        if r['group'] in STATELESS and r['type'] != 'EVENT' and not fresh:
            groups.setdefault(r['group'], []).append((r, g))
        else:
            jobs.append([(r, g)])
    jobs += list(groups.values())
    res_path = os.path.join(AUD, 'sweep_%s.json' % bundle)
    results = json.load(open(res_path)) if os.path.exists(res_path) and only else {}

    scripts = []
    for job in jobs:  # built here, in order (block() keeps per-instance state)
        L = prologue(bundle)
        DIRTY[0], FRESH[0] = False, fresh
        for r, g in job:
            L += block(r, g, bundle, AUD)[0]
        scripts.append(L)

    def run(k):
        job, L = jobs[k], scripts[k]
        txt = harness(L, bundle)
        got = {}
        for line in txt.splitlines():
            m = CMP.match(line)
            if m:
                pid, sc, v = m.group(1).split('|')
                got.setdefault(pid, {}).setdefault(sc, {})[v] = dict(mean=float(m.group(2)), max=int(m.group(3)), changed=int(m.group(4)),
                                                                     cover=(int(m.group(5)), int(m.group(6))),
                                                                     shift=max(abs(float(x)) for x in m.groups()[7:11]) if m.group(8) else None)
        fails = [l for l in txt.splitlines() if 'unknown' in l or 'TIMEOUT' in l or 'NO REF' in l]
        for r, _ in job:
            got.setdefault(r['id'], {})['_log'] = fails[:5]
            if fresh:
                got[r['id']]['_fresh'] = True
        return got

    with ThreadPoolExecutor(max_workers=int(os.environ.get('AUDIT_JOBS', '4'))) as ex:
        for k, got in enumerate(ex.map(run, range(len(jobs)))):
            results.update(got)
            if k % 20 == 0:
                print('%s: %d / %d jobs' % (bundle, k + 1, len(jobs)), flush=True)
                json.dump(results, open(res_path, 'w'), indent=0)
    json.dump(results, open(res_path, 'w'), indent=0)
    print('%s: %d params swept -> %s' % (bundle, len(results), res_path))


def visible(sc):
    """per scene: (visible?, the best value's numbers, noise)"""
    if not sc:
        return False, None, None
    noise = sc.get('noise', dict(changed=0, mean=0))
    best = None
    for v, m in sc.items():
        if v == 'noise' or v.startswith('_'):
            continue
        if best is None or m['changed'] > best[1]['changed']:
            best = (v, m)
    if best is None:
        return False, None, noise
    ok = best[1]['changed'] >= VIS_PX and best[1]['changed'] > VIS_NOISE * noise['changed']
    if not ok and noise['changed'] >= VIS_PX and noise.get('shift') is not None:
        # a chaotic scene (the smoke sim: two identical runs differ by ~40k px): the frame's mean colour shift instead, the same 3x rule
        sh = max((m.get('shift') or 0, v) for v, m in sc.items() if v != 'noise' and not v.startswith('_'))
        ok = sh[0] >= VIS_SHIFT and sh[0] > VIS_NOISE * noise['shift']
        if ok:
            best = (sh[1], dict(sc[sh[1]], rule='shift'))
    return ok, best, noise


def classify(r, sw, g):
    if r.get('static') != 'WIRED':
        return 'DEAD', ''
    if g.get('nonvisual'):
        return 'NON-VISUAL', g['nonvisual']
    if not sw or all(k.startswith('_') for k in sw):
        return 'NOT SWEPT', ''
    s, m, gg = (visible(sw.get(k)) for k in ('S', 'M', 'G'))
    if s[0]:
        return 'VISIBLE ALWAYS', ''
    if m[0]:
        return 'VISIBLE ONLY WHEN MOVING', ''
    if gg[0]:
        return 'VISIBLE ONLY WHEN', g.get('gates', '')
    if g.get('small'):  # a real change at zero noise on a part too small for the 0.1 % rule (stated in the gates file, with numbers)
        sm = visible(sw.get('G'))
        if sm[1] and sm[1][1]['changed'] >= 50 and sm[2]['changed'] == 0:
            return 'VISIBLE ONLY WHEN', (g.get('gates', '') + ' [small: %s]' % g['small'])
    return 'NO VISIBLE EFFECT', ''


def numbers(sw):
    out = []
    for k in ('S', 'M', 'G'):
        ok, best, noise = visible(sw.get(k) if sw else None)
        if best:
            if best[1].get('rule') == 'shift':
                out.append('%s %s: mean shift %.2f/255 (noise %.2f; %d px vs %d)' % (k, best[0], best[1]['shift'], noise['shift'], best[1]['changed'], noise['changed']))
            else:
                out.append('%s %s: %d px (noise %d)' % (k, best[0], best[1]['changed'], noise['changed']))
    return '; '.join(out)


# ---------------------------------------------------------------- Stage 2.4: contact sheets (Copies after LOD Bias)
COPIES_AFTER_LOD = ['Trail Echoes', 'Trail Delay', 'Trail Decay', 'Trail Offset X', 'Trail Offset Y', 'Ghosts', 'Ghost Spacing', 'Ghost Fade',
                    'Trail Smear', 'Trail Blur', 'Trail Dust', 'Trail Grain', 'Trail Drift', 'Trail Drift Dir', 'Trail Fade Curve', 'Trail Glow',
                    'Trail Color', 'Trail Sat', 'Trail Bri', 'Trail Alpha', 'Trail Color Mix']


def sheets(bundle='LoadEhr'):
    st = {r['name']: r for r in json.load(open(os.path.join(AUD, 'static.json')))}
    G = gates()
    sd = os.path.join(AUD, 'copies_%s' % bundle)
    os.makedirs(sd, exist_ok=True)
    L = prologue(bundle)
    tiles = []
    for n in COPIES_AFTER_LOD:
        d, g = st[n], G.get(n, {})
        lo, hi = (0, 2) if d['type'] == 'OPT' else (d['min'], d['max'])
        settle = max(SETTLE, int(g.get('settle_frames', SETTLE)))
        for tag, v in (('min', lo), ('max', hi)):
            p = os.path.join(sd, '%s_%s.png' % (n.replace(' ', '_'), tag))
            L += ['restore_state', 'frames 1', 'hosttime 100', 'advance 0 zero'] + MOTION + clean_recipe(g.get('recipe') or []) + \
                 ['frames %d' % PRE, 'set %s %s' % (n, fmt(v)), 'frames %d' % settle, 'shot ' + p]
            tiles.append((p, '%s %s = %s' % (n, tag, fmt(v))))
    harness(L, 'sheet')
    for part, chunk in (('a', tiles[:22]), ('b', tiles[22:])):
        args = ['montage']
        for p, label in chunk:
            args += ['-label', label, p]
        args += ['-tile', '2x', '-geometry', '+4+4', '-pointsize', '14', '-font', '/System/Library/Fonts/Supplemental/Arial.ttf', '-background', '#202020', '-fill', 'white',
                 os.path.join(AUD, 'copies_%s_sheet_%s.png' % (bundle, part))]
        subprocess.run(args, check=True)
    print('sheets -> %s/copies_%s_sheet_[ab].png' % (AUD, bundle))


# ---------------------------------------------------------------- Stage 3: the map
def needs(rec, name):
    """the gate in words, from the recipe that proved the param visible: the parents and the values that opened it"""
    out = []
    for c in rec:
        w = c.split()
        if w[0] == 'set':
            n, v = ' '.join(w[1:-1]), w[-1]
            out.append('%s = %s' % (n, v))
        elif w[0] == 'pulse':
            out.append('press %s' % ' '.join(w[1:]))
        elif w[0] == 'text':
            k, v = w[1].replace('_', ' '), ' '.join(w[2:])
            out.append('%s = %s' % (k, os.path.basename(os.path.dirname(v)) if v.startswith('/') and k.startswith('Model') else
                                     os.path.basename(v) if v.startswith('/') else '"%s"' % v))
        elif w[0] == 'fft':
            out.append('audio (FFT)')
        elif w[0] == 'bpm':
            out.append('a beat')
        elif w[0] == 'input':
            out.append('a face in the input' if 'face' in c else 'an input layer')
        elif w[0] == 'frames' and int(w[1]) >= 20:
            out.append('a moment (%s frames)' % w[1])
    return ', '.join(dict.fromkeys(out)) or 'motion'


def build_map():
    st = json.load(open(os.path.join(AUD, 'static.json')))
    G = gates()
    sweeps = {}
    for b in BUNDLES:
        p = os.path.join(AUD, 'sweep_%s.json' % b)
        sweeps[b] = json.load(open(p)) if os.path.exists(p) else {}
    md = ['# LoadEhr parameter hierarchy (W10; regenerated W11)', '',
          'Generated by `tools/param_audit.py map` from the static audit (`docs/audit/static.json`), the harness sweeps',
          '(`docs/audit/sweep_<bundle>.json`) and the gates read from the code (`tools/param_audit_gates.json`). Regenerate after any',
          'param change: `tools/param_audit.py all`.', '',
          '**Visibility rule** (one rule for every param): >= 130 px (0.1 % of a 480x270 frame) change by > 8/255 in some channel vs the',
          'param at its default, AND > 3x the default-vs-default noise of the same scene. Scenes: **S** skull, still; **M** S + Spin Y 30 +',
          'Wobble Amount 1 + Pos X 0.4; **G** M + the param\'s gates opened. Numbers = the best test value per scene (changed px, noise px).', '']
    summary, lists = {}, {'DEAD': [], 'BROKEN': [], 'MISROUTED': []}
    for b in BUNDLES:
        md += ['## %s' % b, '']
        cnt = {}
        group = None
        for r in st:
            if r['bundles'][b]['status'] == 'NOT CARRIED':
                continue
            if r['bundles'][b]['status'].startswith('MIS'):
                lists['MISROUTED'].append('%s: %s %s' % (b, r['name'], r['bundles'][b]['status']))
            g = G.get(r['name'], {})
            sw = sweeps[b].get(r['id'])
            cls, why = classify(r, sw, g)
            if cls == 'NO VISIBLE EFFECT':
                acc = (g.get('accepted_in') or {}).get(b) or g.get('accepted')
                cls = 'BROKEN' if not acc else 'NO VISIBLE EFFECT (accepted: %s)' % acc
            cnt[cls.split(' (')[0]] = cnt.get(cls.split(' (')[0], 0) + 1
            if cls == 'BROKEN':
                lists['BROKEN'].append('%s: %s (%s) %s' % (b, r['name'], r['group'], numbers(sw)))
            if cls == 'DEAD':
                lists['DEAD'].append('%s: %s (%s)' % (b, r['name'], r['group']))
            if r['group'] != group:
                group = r['group']
                md += ['', '### %s' % group, '']
            rec = (g.get('recipe_in') or {}).get(b) or g.get('recipe') or []
            need = {'VISIBLE ALWAYS': 'on its own', 'VISIBLE ONLY WHEN MOVING': 'when the model moves',
                    'VISIBLE ONLY WHEN': 'needs ' + needs(rec, r['name'])}.get(cls, cls.lower())
            if cls.startswith('NON-VISUAL'):
                need = 'non-visual: ' + why
            extra = ''
            if g.get('small') and cls == 'VISIBLE ONLY WHEN':
                extra = ' (small: %s)' % g['small']
            if g.get('realtime') and cls == 'VISIBLE ONLY WHEN':
                extra += ' (+ wall time: the face tracker answers in real time)'
            if g.get('applies_to'):
                extra += ' [%s]' % g['applies_to']
            indent = '  ' if rec and cls == 'VISIBLE ONLY WHEN' else ''
            md.append('%s- **%s** — %s%s. %s *%s* %s' % (indent, r['name'], need, extra, g.get('effect', ''), cls,
                                                        ('`' + numbers(sw) + '`') if sw else ''))
        summary[b] = cnt
        md += ['']
    head = ['## Counts', '', '| bundle | ' + ' | '.join(sorted({k for c in summary.values() for k in c})) + ' |',
            '|---|' + '---|' * len({k for c in summary.values() for k in c})]
    keys = sorted({k for c in summary.values() for k in c})
    for b, c in summary.items():
        head.append('| %s | %s |' % (b, ' | '.join(str(c.get(k, 0)) for k in keys)))
    tail = ['## Lists', '']
    for k, v in lists.items():
        tail += ['### %s (%d)' % (k, len(v)), ''] + ['- ' + x for x in v] + ['']
    i = md.index('## LoadEhr')
    md = md[:i] + head + [''] + md[i:] + tail
    open(os.path.join(ROOT, 'docs', 'param_hierarchy.md'), 'w').write('\n'.join(md) + '\n')
    print(json.dumps(summary, indent=1))
    for k, v in lists.items():
        print(k, len(v))
        for x in v[:80]:
            print('  ', x)


# ---------------------------------------------------------------- W11: what a bundle offers but cannot load or use
# Amrit (2026-10-11): "If a plugin cannot load or use something, remove it." Each row: (param, option or None for the whole param) ->
# what the bundle must carry for it to do anything (a GROUP name, or 'FX' = the input layer of an effect bundle), read from the code.
# Options / params with no row need nothing a bundle can lack (beat divisions, axes, qualities, modes of their own group).
REQUIRES = [
    ('Model Source', 'Logo', 'Logo', 'builds slot A from the Logo File (TextLogoSpec)'),
    ('Model Source', 'Text', 'Text', 'builds slot A from the Text group'),
    ('Transition Style', 'Swarm', 'Swarm', 'the fireflies (StyleHere W6: dissolved without the Swarm group)'),
    ('Transition Style', 'Smoke', 'Smoke Sim', 'the sim (W9: Smoke Sim in the Wow bundles only)'),
    ('Transition Style', 'Burn', 'Fire', 'the Fire dials'),
    ('Transition Style', 'Lava', 'Lava', 'the Lava dials'),
    ('Bolt Path', 'To Clones', 'Copies', 'needs clones (fs.inst >= 2): Clone Layout is in Copies'),
    ('Bolt Path', 'Eye To Eye', 'Light C', "Light C's openings"),
    ('Ring From', 'Eyes', 'Light C', "Light C's openings"),
    ('Surf Source', 'Text', 'Text', 'the hidden text slot is built from the Text group'),
    ('Shatter Mode', 'Letters', 'Text', 'one shard per glyph: only text has letters'),
    ('Shatter Mode', 'Pieces + Letters', 'Text', 'one kick per glyph: only text has letters'),
    ('Env Source', 'Input (FX)', 'FX', 'the input layer as the environment'),
    ('Proj Source', 'Input', 'FX', 'the input layer as the picture'),
    ('Project Input', None, 'FX', 'projects the input layer onto the sim smoke'),
] + [('Part %d Skin' % n, None, 'FX', 'the input layer as that part\'s colour (Input Use Skin)') for n in range(1, 9)] + \
    [(n, None, 'FX', 'face tracking reads the input layer') for n in ('Face Track', 'Max Faces', 'Face Scale', 'Face Offset X', 'Face Offset Y',
                                                                      'Follow Roll', 'Follow Yaw', 'Follow Pitch', 'Face Smooth', 'Hide No Face')] + \
    [(n, None, 'FX', 'the input layer') for n in ('Input Use', 'Input Projection', 'Input Mix')]
KEPT = {('Env Source', 'Input (FX)'): 'Amrit 2026-10-11 (W10 call 5): "Keep" (same picture as Studio in a source bundle)'}


def canuse():
    rows, left = [], []
    for b in BUNDLES:
        build = os.environ.get('AUDIT_BUILD', 'build')  # another build's bundles (a before / after)
        dump_rows = harness(['bundle %s/%s.bundle' % (build, b), 'instance', 'params']).splitlines()
        params, groups = {}, set()
        for line in dump_rows:
            m = LINE.match(line)
            if m:
                params[m.group(3)] = [x.strip() for x in (m.group(11) or '').split(' / ')] if m.group(11) else []
                groups.add(m.group(2))
        fx = bool(BUNDLES[b][0])
        for name, opt, need, why in REQUIRES:
            if name not in params or (opt is not None and opt not in params[name]):
                continue  # not offered here
            ok = fx if need == 'FX' else need in groups
            kept = KEPT.get((name, opt))
            rows.append((b, name, opt or '(the param)', need, why, 'ok' if ok else ('KEPT: ' + kept if kept else 'CANNOT USE')))
            if not ok and not kept:
                left.append('%s: %s%s needs %s' % (b, name, ' / ' + opt if opt else '', need))
    md = ['# W11 "cannot use" check (generated by tools/param_audit.py canuse)', '',
          'Amrit 2026-10-11: "If a plugin cannot load or use something, remove it." Every option / param below needs a group (or the FX',
          'input) a bundle may not carry; the check reads what each bundle OFFERS from the harness `params` dump (the host\'s view) and',
          'whether the requirement is carried there. Rows only for what is offered.', '',
          '| bundle | param | option | needs | why | verdict |', '|---|---|---|---|---|---|']
    md += ['| %s | %s | %s | %s | %s | %s |' % r for r in rows]
    md += ['', '**Left: %d**' % len(left)] + ['- ' + x for x in left]
    open(os.environ.get('AUDIT_CANUSE_OUT', os.path.join(AUD, 'canuse.md')), 'w').write('\n'.join(md) + '\n')
    print('canuse: %d offered rows checked, %d kept by Amrit, CANNOT USE left: %d' % (len(rows), sum(r[5].startswith('KEPT') for r in rows), len(left)))
    for x in left:
        print('  ', x)


def main():
    a = sys.argv[1:]
    if not a:
        print(__doc__)
        return
    if a[0] == 'static':
        static()
    elif a[0] == 'sweep':
        fresh = '--fresh' in a
        sweep(a[1], tuple(x for x in a[2:] if x != '--fresh'), fresh)
    elif a[0] == 'sheets':
        sheets(*(a[1:2] or ['LoadEhr']))
    elif a[0] == 'map':
        build_map()
    elif a[0] == 'canuse':
        canuse()
    elif a[0] == 'all':
        static()
        for b in BUNDLES:
            sweep(b)
        build_map()
        canuse()


if __name__ == '__main__':
    main()
