#!/usr/bin/env python3
"""Regenerates the CLAUDE.md parameter index map from the harness `params` dump of the four bundles (W26: LoadEhr, LoadEhr FX,
LoadEhr Wow, LoadEhr Wow FX; each column = that bundle's FFGL index, "—" = the bundle does not carry the param).

usage: tools/param_map.py            (after a Release build; rewrites the table in CLAUDE.md in place)

The table is built field by field from the parsed dump: an index is always printed as an index and a type
code is only ever turned into a type NAME in the Type column. (The hand-made W3 map had "HUE" in the index
column of row 200, because a text substitution of type codes also hit the index 200 == FF_TYPE_HUE.)
"""
import os
import re
import subprocess
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TYPES = {0: 'BOOL', 1: 'EVENT', 10: 'STD', 11: 'OPT', 12: 'BUFFER', 13: 'INT', 14: 'FILE', 100: 'TEXT',
         200: 'HUE', 201: 'SAT', 202: 'BRI', 203: 'ALPHA'}
LINE = re.compile(r'^\s+(\d+) \| (.*?)\s*\| (.*?)\s*\| (.*?)\s*\| type\s+(\d+) \| range (\S+?)\.\.(\S+) \| default (\S+) \| '
                  r'elements (\d+) \| (visible|hidden)(?: \| options (.*))?$')


def dump(bundle):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(f'bundle {bundle}\ninstance\nparams\n')
        script = f.name
    out = subprocess.run([os.path.join(ROOT, 'build/loadehr_harness'), script], cwd=ROOT, capture_output=True, text=True).stdout
    os.unlink(script)
    rows = []
    for line in out.splitlines():
        m = LINE.match(line)
        if m:
            i, group, name, _disp, t, lo, hi, d, _n, vis, opts = m.groups()
            rows.append(dict(i=int(i), group=group, name=name, type=int(t), lo=lo, hi=hi, d=d, hidden=vis == 'hidden', opts=opts))
    return rows


def rng(r):
    t = r['type']
    if t == 11:
        return r['opts'] or '-'
    if t == 14:
        return 'fbx obj gltf glb'
    if t == 12:
        return '512 bins (FFT)'
    if t in (0, 1, 100):
        return '-'
    return f"{r['lo']}..{r['hi']}"


def main():
    cols = [dump('build/%s.bundle' % b) for b in ('LoadEhr', 'LoadEhrFX', 'LoadEhrWow', 'LoadEhrWowFX')]
    idx = [{r['name']: r['i'] for r in c} for c in cols]
    # every bundle carries a subsequence of ParamList.inc: merge the two FX dumps (each a superset of its source) in that order
    a, b = cols[1], cols[3]
    an, bn = set(idx[1]), set(idx[3])
    rows, i, j = [], 0, 0
    while i < len(a) or j < len(b):
        if i < len(a) and j < len(b) and a[i]['name'] == b[j]['name']:
            rows.append(a[i]); i += 1; j += 1
        elif i < len(a) and a[i]['name'] not in bn:
            rows.append(a[i]); i += 1
        else:
            rows.append(b[j]); j += 1
    lines = ['| LoadEhr | FX | Wow | Wow FX | Group | Name | Type | Range | Default |', '|---|---|---|---|---|---|---|---|---|']
    for r in rows:
        t = TYPES.get(r['type'], str(r['type'])) + (' (hidden)' if r['hidden'] and r['type'] == 100 else '')
        d = '-' if r['type'] in (1, 12, 14, 100) else r['d']
        n = ' | '.join('—' if r['name'] not in x else str(x[r['name']]) for x in idx)
        lines.append(f"| {n} | {r['group']} | {r['name']} | {t} | {rng(r)} | {d} |")
    table = '\n'.join(lines)
    path = os.path.join(ROOT, 'CLAUDE.md')
    md = open(path).read()
    start = md.index('| LoadEhr | FX | Wow | Wow FX |') if '| LoadEhr | FX |' in md else md.index('| # src | # FX |')
    end = md.index('\n\n', start)
    md = md[:start] + table + md[end:]
    n = [len(c) for c in cols]
    md = re.sub(r'\*\*(Source \d+ params, FX \d+|LoadEhr \d+ params, FX \d+, Wow \d+, Wow FX \d+)\.\*\*',
                f'**LoadEhr {n[0]} params, FX {n[1]}, Wow {n[2]}, Wow FX {n[3]}.**', md)
    open(path, 'w').write(md)
    print(f'CLAUDE.md param map: {len(rows)} rows; LoadEhr {n[0]}, FX {n[1]}, Wow {n[2]}, Wow FX {n[3]}')


if __name__ == '__main__':
    main()
