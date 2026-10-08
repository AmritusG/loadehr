#!/usr/bin/env python3
"""LoadEhr W6: dial the W6 clips (LoadEhr Wow source clips) over REST by param NAME. Never triggers.
usage: tools/arena_w6.py points <layer> [col]        layer "LoadEhr W6 Points": every Point Source on the skull, Points + Wire, text
                                                    with Point Twinkle, points + Shatter On Beat
       tools/arena_w6.py trans <layer> [col]         layer "LoadEhr W6 Transitions": one clip per new style (5..22), 2 slots, Ping-Pong,
                                                    6 s per hop, Hold 1.5 s; + a 4-slot Loop with Style Order Cycle
       tools/arena_w6.py get <layer> <col> [name..]  read back"""
import colorsys, json, sys, urllib.request

API = 'http://127.0.0.1:8080/api/v1'
LIB = '/Users/amritrosell/Documents/Resolume Arena/MaskEhr Library'
M = {
    'skull': LIB + '/Skulls/Human Skull/scene.gltf', 'helmet': LIB + '/Helmets/SciFi Helmet/SciFiHelmet.gltf',
    'crown': LIB + '/Crowns/Crown 1918/scene.gltf', 'nefertiti': LIB + '/Heads/Nefertiti/scene.gltf',
    'anubis': LIB + '/Heads/Anubis Gold/scene.gltf', 'heart': LIB + '/Anatomy/Heart/scene.gltf', 'brain': LIB + '/Anatomy/Brain/scene.gltf',
    'astronaut': LIB + '/Helmets/Astronaut Helmet/scene.gltf', 'golden': LIB + '/Crowns/Golden Crown/scene.gltf',
    'plague': LIB + '/Masks/Plague Mask/scene.gltf', 'gas': LIB + '/Masks/Gas Mask/scene.gltf', 'bull': LIB + '/Skulls/Bull Skull/scene.gltf',
    'crystal': LIB + '/Skulls/Crystal Skull/scene.gltf', 'deer': LIB + '/Skulls/Deer Skull/scene.gltf', 'lion': LIB + '/Skulls/Lion Skull/scene.gltf',
    'ganesha': LIB + '/Masks/Ganesha Mask/scene.gltf', 'delft': LIB + '/Masks/Delft Mask/scene.gltf', 'ram': LIB + '/Skulls/Ram Skull/scene.gltf',
}
assert not any('arnival' in p for p in M.values())  # HARD RULE 3
SOFT = [('Spin Y', 3), ('Key Intensity', 0.8)]
CYAN = [('Wire Color', (0.5, 1, 1, 1))]  # one Arena picker: (hue, sat, bri, alpha = Wire Color Mix)
PT = SOFT + CYAN + [('Wireframe Mix', 1), ('Wire Thickness', 1.2), ('Wire Glow', 2), ('Model Source', 'Model File')]
POINTS = {
    1: ('W6 Points Vertices', PT + [('Model A', M['skull']), ('Wire Mode', 'Points Only'), ('Point Source', 'Vertices')]),
    2: ('W6 Points Feature', PT + [('Model A', M['skull']), ('Wire Mode', 'Points Only'), ('Point Source', 'Feature Vertices')]),
    3: ('W6 Points 25k', PT + [('Model A', M['skull']), ('Wire Mode', 'Points Only'), ('Point Source', 'Surface 25k')]),
    4: ('W6 Points 100k', PT + [('Model A', M['skull']), ('Wire Mode', 'Points Only'), ('Point Source', 'Surface 100k')]),
    5: ('W6 Points 400k', PT + [('Model A', M['skull']), ('Wire Mode', 'Points Only'), ('Point Source', 'Surface 400k')]),
    6: ('W6 Points + Wire', SOFT + [('Model Source', 'Model File'), ('Model A', M['helmet']), ('Wire Mode', 'Points + Wire'), ('Wireframe Mix', 1),
                                    ('Point Source', 'Feature Vertices'), ('Wire Edges', 'Feature Edges'), ('Wire Color', (0.08, 0.7, 1, 0.8)),
                                    ('Wire Glow', 1.5), ('Wire Thickness', 1.2)]),
    7: ('W6 Points Text Twinkle', SOFT + CYAN + [('Model Source', 'Text'), ('Text', 'LOADEHR'), ('Wire Mode', 'Points Only'), ('Wireframe Mix', 1),
                                                 ('Point Source', 'Surface 100k'), ('Point Twinkle', 0.8), ('Wire Glow', 2), ('Wire Thickness', 1.2)]),
    8: ('W6 Points Shatter', PT + [('Model A', M['skull']), ('Wire Mode', 'Points Only'), ('Point Source', 'Surface 100k'),
                                   ('Shatter On Beat', 'Every 4')]),
}
HOP = SOFT + [('Model Source', 'Model File'), ('Transition Play', 'Ping-Pong'), ('Transition Time', 6), ('Hold Time', 1.5), ('Chain Loop', False),
              ('Style Order', 'Fixed')]
PAIRS = {5: ('skull', 'helmet'), 6: ('anubis', 'nefertiti'), 7: ('astronaut', 'gas'), 8: ('bull', 'heart'), 9: ('crystal', 'golden'),
         10: ('deer', 'plague'), 11: ('lion', 'brain'), 12: ('ram', 'crown'), 13: ('ganesha', 'skull'), 14: ('helmet', 'nefertiti'),
         15: ('plague', 'anubis'), 16: ('delft', 'astronaut'), 17: ('skull', 'crown'), 18: ('gas', 'bull'), 19: ('heart', 'ganesha'),
         20: ('nefertiti', 'deer'), 21: ('golden', 'lion'), 22: ('brain', 'ram')}
STYLES = ['Dissolve', 'Shatter', 'Dust', 'Slice', 'Liquid', 'Points', 'Shrinkwrap', 'Wire', 'Voxel', 'Low Poly', 'Pieces', 'Implode', 'Twist', 'Melt',
          'Scan', 'Glitch', 'Ripple', 'Swarm', 'Smoke', 'Burn', 'Lava', 'Spin Swap', 'Fold']
EXTRA = {5: CYAN + [('Wire Glow', 1.5), ('Wire Thickness', 1.2)], 7: CYAN + [('Wire Glow', 1)], 14: CYAN + [('Wire Glow', 2)], 16: CYAN + [('Wire Glow', 1)]}
TRANS = {st - 4: ('W6 %s' % STYLES[st], HOP + [('Model A', M[PAIRS[st][0]]), ('Model B', M[PAIRS[st][1]]), ('Model C', ''), ('Model D', ''),
                                                ('Transition Style', STYLES[st])] + EXTRA.get(st, [])) for st in range(5, 23)}
TRANS[19] = ('W6 Cycle All Styles', SOFT + [('Model Source', 'Model File'), ('Model A', M['skull']), ('Model B', M['helmet']), ('Model C', M['crown']),
                                            ('Model D', M['nefertiti']), ('Chain Loop', True), ('Transition Play', 'Loop'), ('Style Order', 'Cycle'),
                                            ('Transition Style', 'Points'), ('Transition Time', 6), ('Hold Time', 1.5)] + CYAN + [('Wire Glow', 1)])


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


def dial(table, layer, col):
    c = req('GET', '/composition/layers/%d/clips/%d' % (layer, col))
    name, dials = table[col]
    sp = c['video']['sourceparams']
    for k, v in dials:
        put(sp, k, v)
    req('PUT', '/composition/layers/%d/clips/%d' % (layer, col), {'name': {'value': name}})
    print('C%d %s: %d dials' % (col, name, len(dials)))


if __name__ == '__main__':
    what, layer = sys.argv[1], int(sys.argv[2])
    if what == 'get':
        sp = req('GET', '/composition/layers/%d/clips/%s' % (layer, sys.argv[3]))['video']['sourceparams']
        for n in sys.argv[4:] or sorted(sp):
            print('%s = %r' % (n, sp.get(n, {}).get('value', '<missing>')))
        sys.exit(0)
    table = POINTS if what == 'points' else TRANS
    for col in ([int(sys.argv[3])] if len(sys.argv) > 3 else sorted(table)):
        dial(table, layer, col)
