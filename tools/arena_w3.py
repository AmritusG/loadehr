#!/usr/bin/env python3
"""LoadEhr W3: dial the W3 clips (layer "LoadEhr W3") over REST by param NAME. Never triggers a clip.
usage: tools/arena_w3.py <layer>          every clip below (columns 1-11)
       tools/arena_w3.py <layer> <col>    one column
Columns 1-7, 10, 11: LoadEhr Wow source clips (Proj File). 8, 9: the DXV file as the clip's source with LoadEhr Wow FX on it, same
dials: 8 = Proj Source File (DXVKit decodes the file), 9 = Proj Source Input (Arena's own DXV decoder feeds the input)."""
import json, os, sys, urllib.request

API = 'http://127.0.0.1:8080/api/v1'
SKULL = '/Users/amritrosell/Documents/Resolume Arena/MaskEhr Library/Skulls/Human Skull/scene.gltf'
MOV = os.path.expanduser('~/Movies')
DXT1 = MOV + '/KLV Set/06_THE GREAT SURRENDER_HQ - No Alpha No Audio.mov'
DXT5A = MOV + '/Testfiles/DXT5-DXV3.mov'  # DXT5 with real transparency (black_snowflake's DXT5 alpha is opaque in every frame)
YCG6 = MOV + '/KLV Set/01_AWAKE & DISSOLVE_HQ - No Alpha No Audio.mov'
YG10 = MOV + '/Testfiles/YG10-DXV3.mov'
LEGACY = MOV + '/Testfiles/Metric_01.mov'
DXT5_4K = MOV + '/black_snowflake_DXV Normal Quality With Alpha.mov'
PORTRAIT = MOV + '/S06 Lone Star Halo 5s_Custom.mov'  # YCG6 908x2276 (coded 912x2288: padding on both axes)
# the skull in haze, soft (the W2 projector clips' dials)
PROJ = [('Model A', SKULL), ('Proj Enable', 1), ('Proj Pos X', 0.6), ('Proj Pos Y', 0.6), ('Proj Pos Z', 4), ('Proj Throw', 28),
        ('Proj Intensity', 20), ('Proj Beam', 1.0), ('Haze', 0.35), ('Key Intensity', 0.8), ('Video Loop', 'Loop'), ('Spin Y', 3)]
CLIPS = {
    1: PROJ + [('Proj File', DXT1)],
    2: PROJ + [('Proj File', DXT5A)],
    3: PROJ + [('Proj File', YCG6)],
    4: PROJ + [('Proj File', YG10)],
    5: PROJ + [('Proj File', LEGACY)],
    6: PROJ + [('Proj File', DXT5_4K)],
    7: PROJ + [('Proj File', PORTRAIT), ('Proj Throw', 14)],
    8: PROJ + [('Proj Source', 'File'), ('Proj File', DXT5A), ('Input Use', 'Off')],
    9: PROJ + [('Proj Source', 'Input'), ('Input Use', 'Off')],
    10: PROJ + [('Proj File', DXT5A), ('Video Alpha', 'Straight')],
    11: PROJ + [('Proj File', DXT5A), ('Video Alpha', 'Premultiplied')],
}
FX = (8, 9)


def req(method, path, body=None):
    r = urllib.request.Request(API + path, method=method, data=None if body is None else json.dumps(body).encode(),
                               headers={'Content-Type': 'application/json'})
    with urllib.request.urlopen(r, timeout=10) as f:
        t = f.read()
        return json.loads(t) if t else None


def put(sp, name, value):
    p = sp.get(name)
    if p is None:
        print('  MISSING', name)
        return
    t = p['valuetype']
    if t == 'ParamChoice':
        val = value if isinstance(value, str) else p['options'][int(round(value))]
    elif t == 'ParamBoolean':
        val = bool(value)
    elif t in ('ParamString', 'ParamFile', 'ParamText'):
        val = value
    else:
        val = float(value)
    req('PUT', '/parameter/by-id/%d' % p['id'], {'value': val})


def dial(layer, col):
    c = req('GET', '/composition/layers/%d/clips/%d' % (layer, col))
    if col in FX:
        sp = next(e['params'] for e in c['video']['effects'] if e['name'].startswith('LoadEhr Wow FX'))
    else:
        sp = c['video']['sourceparams']
    for k, v in CLIPS[col]:
        put(sp, k, v)
    print('C%d %s: %d dials' % (col, c['name']['value'], len(CLIPS[col])))


if __name__ == '__main__':
    layer = int(sys.argv[1])
    for col in ([int(sys.argv[2])] if len(sys.argv) > 2 else sorted(CLIPS)):
        dial(layer, col)
