#!/usr/bin/env python3
"""LoadEhr W9: dial the W9 clips over REST by param NAME (one layer per stage). Never triggers.
usage: tools/arena_w9.py <stage A|B|C|D|E> <layer> [col]     the clips must exist (LoadEhr / LoadEhr Wow source, see SOURCE)
       tools/arena_w9.py get <layer> <col> [name ...]"""
import os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import arena_w6 as A  # noqa: E402

M = A.M
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LOGO = os.path.join(ROOT, 'test_logos', 'test_emblem.svg')
DXV = os.path.join(ROOT, 'test_media', 'gen', 'w4', 'code_dxv_1080.mov')
SOFT = A.SOFT + [('Model Source', 'Model File'), ('Model B', ''), ('Model C', ''), ('Model D', '')]
CYAN = [('Wire Color', (0.5, 1, 1, 0.6))]


def m(a, *extra):
    return SOFT + [('Model A', M[a])] + list(extra)


def text(t, *extra):
    return A.SOFT + [('Model Source', 'Text'), ('Text', t), ('Model B', ''), ('Model C', ''), ('Model D', '')] + list(extra)


STAGES = {
    'A': {
        1: ('W9 Plexus', m('skull', ('Line Look', 'Plexus'), *CYAN)),
        2: ('W9 Light Paint', m('helmet', ('Line Look', 'Light Paint'), ('Spin Y', 12), ('Line Amount', 0.4), *CYAN)),
        3: ('W9 Neon Tubes Text', text('LOADEHR', ('Line Look', 'Neon Tubes'), ('Wire Color', (0.9, 0.85, 1, 1)), ('Line Speed', 0.4))),
        4: ('W9 Plexus Audio', m('nefertiti', ('Line Look', 'Plexus'), ('Line Audio', 2), ('Line Speed', 1), *CYAN)),
        5: ('W9 Lines Too Much', m('skull', ('Line Look', 'Plexus'), ('Line Amount', 1), ('Line Reach', 1), ('Line Speed', 4), ('Wire Glow', 3), *CYAN)),
    },
    'B': {
        1: ('W9 Slit-Scan', m('skull', ('Build Look', 'Slit-Scan'), ('Spin Y', 15), ('Build Amount', 0.5))),
        2: ('W9 Print On', m('helmet', ('Build Look', 'Print On'), ('Build Speed', 4), ('Build Amount', 1), ('Orbit Pitch', 25),
                              ('Wire Mode', 'Wire Only'), ('Wireframe Mix', 0.3), *CYAN)),
        3: ('W9 Exploded Text', text('LOADEHR', ('Build Look', 'Exploded'), ('Build Amount', 0.4), ('Build On Beat', 'Every 2'))),
        4: ('W9 Black Hole', m('skull', ('Build Look', 'Black Hole'), ('Build Speed', 3), ('Build Amount', 0.8))),
        5: ('W9 Build Too Much', m('skull', ('Build Look', 'Slit-Scan'), ('Spin Y', 30), ('Build Amount', 1), ('Build Speed', 20))),
    },
    'C': {
        1: ('W9 Cross-Section', m('skull', ('Cut Look', 'Cross-Section'), ('Build Axis', 'Z'), ('Cut Size', 1), ('Cut Speed', 4), *CYAN)),
        2: ('W9 Reveal Box', m('helmet', ('Cut Look', 'Reveal Box'), ('Cut Speed', 4), ('Cut Size', 0.4), ('Cut Position', 0.4), *CYAN)),
        3: ('W9 Window Slot B', m('skull', ('Model B', M['helmet']), ('Cut Look', 'Window'), ('Cut Speed', 3), ('Cut Size', 0.5))),
        4: ('W9 Window Video', m('skull', ('Cut Look', 'Window'), ('Proj File', DXV), ('Proj Enable', True), ('Proj Beam', 0), ('Proj Surface', 0.3))),
        5: ('W9 Cross-Section Smoke', m('skull', ('Cut Look', 'Cross-Section'), ('Build Axis', 'Z'), ('Cut Size', 1), ('Cut Speed', 3),
                                        ('Smoke Engine', 'Sim'), ('Emit Inside', 3), ('Smoke Ambient', 1.2), ('Haze', 0), *CYAN)),
        6: ('W9 Cut Too Much', m('skull', ('Cut Look', 'Cross-Section'), ('Cut Size', 0.05), ('Cut Speed', 30), *CYAN)),
    },
    'D': {
        1: ('W9 Audio Spikes', m('skull', ('Surf Look', 'Audio Spikes'), ('Surf Amount', 0.8))),
        2: ('W9 Skin Of Copies', m('skull', ('Model B', M['helmet']), ('Surf Look', 'Skin Of Copies'), ('Surf Source', 'Slot B'), ('Surf Scale', 0.5),
                                   ('Surf Amount', 1))),
        3: ('W9 X-Ray', m('helmet', ('Surf Look', 'X-Ray'), ('Surf Amount', 1))),
        4: ('W9 Thermal', m('skull', ('Surf Look', 'Thermal'), ('Surf Amount', 1))),
        5: ('W9 Night Vision', m('skull', ('Surf Look', 'Night Vision'), ('Surf Amount', 1))),
        6: ('W9 LED Matrix', m('helmet', ('Surf Look', 'LED Matrix'), ('Surf Amount', 1), ('Surf Scale', 0.2))),
        7: ('W9 Surface Too Much', m('skull', ('Surf Look', 'Audio Spikes'), ('Surf Amount', 2), ('Surf Scale', 1), ('Surf Speed', 4))),
    },
    'E': {
        1: ('W9 Shape Cube', m('skull', ('Shape Target', 'Cube'), ('Spherize', 0.6))),
        2: ('W9 Shape Torus', m('helmet', ('Shape Target', 'Torus'), ('Spherize', 0.6))),
        3: ('W9 Shape Crystal Text', text('LOADEHR', ('Shape Target', 'Crystal'), ('Spherize', 0.6))),
        4: ('W9 Shape Sphere', m('skull', ('Shape Target', 'Sphere'), ('Spherize', 0.6))),
        5: ('W9 Shape Too Much', m('skull', ('Shape Target', 'Torus'), ('Spherize', 1), ('Deform Amount', 2), ('Twist', 2))),
    },
}
# which clips need the Wow bundle (the sim, the projector)
SOURCE = {('C', 4): 'LoadEhr Wow', ('C', 5): 'LoadEhr Wow'}
assert not any('arnival' in str(v) for st in STAGES.values() for _, d in st.values() for _, v in d)  # HARD RULE 3

if __name__ == '__main__':
    if sys.argv[1] == 'get':
        sp = A.req('GET', '/composition/layers/%s/clips/%s' % (sys.argv[2], sys.argv[3]))['video']['sourceparams']
        for n in sys.argv[4:] or sorted(sp):
            print('%s = %r' % (n, sp.get(n, {}).get('value', '<missing>')))
        sys.exit(0)
    table, layer = STAGES[sys.argv[1]], int(sys.argv[2])
    for col in ([int(sys.argv[3])] if len(sys.argv) > 3 else sorted(table)):
        A.dial(table, layer, col)
