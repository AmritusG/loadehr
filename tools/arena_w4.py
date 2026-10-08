#!/usr/bin/env python3
"""LoadEhr W4: dial the W4 scrub clips (layer "LoadEhr W4") over REST by param NAME. Never triggers a clip. The parameter-timeline
animations on Video Position (C1, C2) are Arena-side (arena MCP `parameter set_animation`), not set here.
usage: tools/arena_w4.py <layer>            every clip below
       tools/arena_w4.py <layer> <col>      one column
       tools/arena_w4.py <layer> <col> get  read back Video Position / Video Time / Video Loop of that clip"""
import json, os, sys, urllib.request

API = 'http://127.0.0.1:8080/api/v1'
SKULL = '/Users/amritrosell/Documents/Resolume Arena/MaskEhr Library/Skulls/Human Skull/scene.gltf'
MOV = os.path.expanduser('~/Movies')
GEN = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'test_media', 'gen')
SNOW = MOV + '/black_snowflake_DXV Normal Quality With Alpha.mov'  # DXV DXT5 4K, 451 frames, 15.03 s (W4: ~/Movies/KLV Set vanished mid-session)
YCG6 = MOV + '/Testfiles/YCG6-DXV3.mov'                                  # DXV YCG6 1080p, 10 s
HAP = GEN + '/hap_1080.mov'                                              # HAP 1080p, 6 s
H264 = GEN + '/h264_1080.mp4'                                            # H.264 1080p, 6 s (one key frame)
# the skull in haze, soft (the W2 / W3 projector clips' dials)
PROJ = [('Model A', SKULL), ('Proj Enable', 1), ('Proj Pos X', 0.6), ('Proj Pos Y', 0.6), ('Proj Pos Z', 4), ('Proj Throw', 28),
        ('Proj Intensity', 20), ('Proj Beam', 1.0), ('Haze', 0.35), ('Key Intensity', 0.8), ('Spin Y', 3), ('Video Speed', 1)]
CLIPS = {
    1: ('W4 Scrub Transport', PROJ + [('Proj File', SNOW), ('Video Loop', 'Scrub'), ('Video Position', 0)]),       # + timeline Play Once & Clear (holds the end), 15.0333 s
    2: ('W4 Scrub BPM Loop', PROJ + [('Proj File', YCG6), ('Video Loop', 'Scrub'), ('Video Position', 0)]),        # + timeline BPM sync, loop
    3: ('W4 Scrub HAP', PROJ + [('Proj File', HAP), ('Video Loop', 'Scrub'), ('Video Position', 0.5)]),
    4: ('W4 Scrub H264', PROJ + [('Proj File', H264), ('Video Loop', 'Scrub'), ('Video Position', 0.5)]),
    5: ('W4 Loop Playhead', PROJ + [('Proj File', SNOW), ('Video Loop', 'Loop')]),
    6: ('W4 Reverse PingPong', PROJ + [('Proj File', YCG6), ('Video Loop', 'Ping-Pong'), ('Video Speed', -1)]),
}


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
    name, dials = CLIPS[col]
    sp = c['video']['sourceparams']
    for k, v in dials:
        put(sp, k, v)
    req('PUT', '/composition/layers/%d/clips/%d' % (layer, col), {'name': {'value': name}})
    print('C%d %s: %d dials' % (col, name, len(dials)))


def get(layer, col):
    sp = req('GET', '/composition/layers/%d/clips/%d' % (layer, col))['video']['sourceparams']
    for k in ('Video Loop', 'Video Position', 'Video Time', 'Video Speed'):
        print('%-15s %s' % (k, sp.get(k, {}).get('value')))


if __name__ == '__main__':
    layer = int(sys.argv[1])
    if len(sys.argv) > 3 and sys.argv[3] == 'get':
        get(layer, int(sys.argv[2]))
    else:
        for col in ([int(sys.argv[2])] if len(sys.argv) > 2 else sorted(CLIPS)):
            dial(layer, col)
