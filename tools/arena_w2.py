#!/usr/bin/env python3
"""LoadEhr W2: dial the W2 clips (layer "LoadEhr W2") over REST by param NAME. Never triggers a clip.
usage: tools/arena_w2.py <layer>          every clip below (columns 1-16)
       tools/arena_w2.py <layer> <col>    one column
Colours: '<X> Color' = (hue, sat, bri, alpha) -> the one Arena picker as #rrggbbaa."""
import colorsys, json, os, sys, urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
API = 'http://127.0.0.1:8080/api/v1'
SKULL = '/Users/amritrosell/Documents/Resolume Arena/MaskEhr Library/Skulls/Human Skull/scene.gltf'
SVG = os.path.join(ROOT, 'test_logos', 'test_emblem.svg')
SOFT = [('Spin Y', 4), ('Orbit Yaw', 12)]
TEXT = [('Model Source', 'Text'), ('Text', 'LOADEHR')]
GOLD = [('Tint Color', (0.11, 0.65, 1.0, 1.0)), ('Metal Override', 1), ('Metallic Value', 1), ('Roughness Scale', 0.6)]
SHATTER = [('Shatter On Beat', 'Every 4'), ('Shard Count', 64), ('Shatter Force', 0.8), ('Shatter Gravity', 0.8), ('Shard Spin', 1.2)]
CLIPS = {
    1: TEXT + GOLD + SHATTER + [('Shatter Mode', 'Pieces'), ('Part 5 Glow', 1.2), ('Part 5 Hue Shift', 0.93), ('Orbit Yaw', 15)],
    2: TEXT + GOLD + SHATTER + [('Shatter Mode', 'Letters'), ('Orbit Yaw', 15)],
    3: [('Model Source', 'Logo'), ('Logo File', SVG), ('Shatter Mode', 'Pieces'), ('Shard Count', 64), ('Shatter Physics', 'Bounce'),
        ('Bounciness', 0.55), ('Floor Friction', 0.3), ('Shatter On Beat', 'Every 8'), ('Shatter Force', 0.9), ('Shatter Gravity', 3),
        ('Shard Spin', 1.5), ('Shadow Catcher', 0.6), ('Orbit Yaw', 15)],
    4: [('Model A', SKULL), ('Taper', 1.0)] + SOFT,
    5: [('Model A', SKULL), ('Squash Beat', 1.0)] + SOFT,
    6: [('Model A', SKULL), ('Shear', 0.6)] + SOFT,
    7: [('Model A', SKULL), ('Bulge', 0.8)] + SOFT,
    8: [('Model A', SKULL), ('Spherize', 0.6)] + SOFT,
    9: [('Model A', SKULL), ('Noise Warp', 0.6), ('Warp Scale', 2.5), ('Warp Speed', 0.3)] + SOFT,
    10: [('Model A', SKULL), ('Jelly', 1.5), ('Wobble Amount', 1.0), ('Wobble Speed', 0.6), ('Beat Bounce', 0.6), ('Orbit Yaw', 12)],
    11: [('Model A', SKULL), ('Glitch Shift', 0.3), ('Glitch Bands', 14), ('Glitch Rate', 3)] + SOFT,
    12: [('Model A', SKULL), ('Mirror Fold', 'X')] + SOFT,
    13: [('Model A', SKULL), ('Deform Amount', 2), ('Taper', 0.6), ('Twist', 1.5), ('Bend', 0.6), ('Squash', 0.3), ('Shear', 0.4), ('Bulge', 0.5),
         ('Spherize', 0.3), ('Noise Warp', 0.5), ('Warp Speed', 0.3), ('Ripple', 0.03), ('Glitch Shift', 0.2), ('Glitch Rate', 3),
         ('Mirror Fold', 'X'), ('Jelly', 1)] + SOFT,
    14: TEXT + [('Twist', 1.0), ('Deform Axis', 'X'), ('Noise Warp', 0.3), ('Warp Speed', 0.3), ('Orbit Yaw', 15)],
}
PROJ = [('Model A', SKULL), ('Proj Enable', 1), ('Proj Pos X', 0.6), ('Proj Pos Y', 0.6), ('Proj Pos Z', 4), ('Proj Throw', 28),
        ('Proj Intensity', 20), ('Proj Beam', 1.0), ('Haze', 0.35), ('Key Intensity', 0.8), ('Video Loop', 'Loop'), ('Spin Y', 3)]
CLIPS[15] = PROJ + [('Proj File', os.path.join(ROOT, 'test_media', 'faces_clip.mp4'))]
CLIPS[16] = PROJ + [('Proj File', os.path.join(ROOT, 'test_media', 'gen', 'hap_1080.mov'))]


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
    sp = c['video']['sourceparams']
    for k, v in CLIPS[col]:
        put(sp, k, v)
    print('C%d %s: %d dials' % (col, c['name']['value'], len(CLIPS[col])))


if __name__ == '__main__':
    layer = int(sys.argv[1])
    for col in ( [int(sys.argv[2])] if len(sys.argv) > 2 else sorted(CLIPS) ):
        dial(layer, col)
