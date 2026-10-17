#!/usr/bin/env python3
"""LoadEhr W5: dial the W5 wireframe clips (layer "LoadEhr W5", LoadEhr Wow source clips) over REST by param NAME. Never triggers.
usage: tools/arena_w5.py <layer>          every clip below
       tools/arena_w5.py <layer> <col>    one column"""
import colorsys, json, os, sys, urllib.request

API = 'http://127.0.0.1:8080/api/v1'
LIB = os.path.expanduser('~/Documents/Resolume Arena/MaskEhr Library')
SKULL = LIB + '/Skulls/Human Skull/scene.gltf'
HELMET = LIB + '/Helmets/SciFi Helmet/SciFiHelmet.gltf'
SOFT = [('Spin Y', 3), ('Key Intensity', 0.8), ('Wireframe Mix', 1), ('Wire Thickness', 1.2)]
# a colour = ONE Arena picker under the HUE name: (hue, sat, bri, alpha) -> '#rrggbbaa' (alpha = Wire Color Mix)
CYAN = [('Wire Color', (0.5, 1, 1, 1))]
SKULL_ONLY = SOFT + CYAN + [('Model Source', 'Model File'), ('Model A', SKULL), ('Wire Mode', 'Wire Only'), ('Wire Edges', 'Feature Edges'),
                            ('Wire Glow', 3), ('Bloom', 1.5)]
TEXT = [('Model Source', 'Text'), ('Text', 'LOADEHR')]
CLIPS = {
    1: ('W5 Wire Only Skull', SKULL_ONLY + [('Wire Back', 'Hidden')]),
    2: ('W5 Wire Only XRay', SKULL_ONLY + [('Wire Back', 'X-Ray')]),
    3: ('W5 Wire Text Bloom', SOFT + CYAN + TEXT + [('Wire Mode', 'Wire Only'), ('Wire Bloom', 3), ('Wire Bloom Size', 0.5), ('Wire Glow', 1)]),
    4: ('W5 Dark Fill Helmet', SOFT + [('Model Source', 'Model File'), ('Model A', HELMET), ('Wire Mode', 'Wire + Dark Fill'), ('Wire Edges', 'Feature Edges'),
                                       ('Wire Color', (0.08, 0.7, 1, 0.8)), ('Wire Glow', 1.5)]),
    5: ('W5 Over Surface Glow', SOFT + [('Model Source', 'Model File'), ('Model A', SKULL), ('Wire Mode', 'Over Surface'), ('Wireframe Mix', 0.6),
                                        ('Wire Glow', 2)]),
    6: ('W5 Wire Pieces', SOFT + CYAN + TEXT + [('Wire Mode', 'Wire Only'), ('Wire Edges', 'Feature Edges'), ('Noise Warp', 0.5),
                                                ('Shatter Mode', 'Pieces'), ('Shatter On Beat', 'Every 4'), ('Wire Glow', 2)]),
    7: ('W5 Too Much', SKULL_ONLY + [('Wire Edges', 'All Triangles'), ('Wire Glow', 10), ('Wire Bloom', 10), ('Wire Bloom Size', 1)]),
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
    if isinstance(value, tuple):
        r, g, b = colorsys.hsv_to_rgb(value[0] % 1.0, value[1], value[2])
        val = '#%02x%02x%02x%02x' % (round(r * 255), round(g * 255), round(b * 255), round(value[3] * 255))
    elif t == 'ParamChoice':
        val = value if isinstance(value, str) else p['options'][int(round(value))]
    elif t == 'ParamBoolean':
        val = bool(value)
    elif t in ('ParamString', 'ParamFile', 'ParamText', 'ParamColor'):
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


if __name__ == '__main__':
    layer = int(sys.argv[1])
    for col in ([int(sys.argv[2])] if len(sys.argv) > 2 else sorted(CLIPS)):
        dial(layer, col)
