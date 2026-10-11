#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""LoadEhr W11: dial the decision clips (layer "LoadEhr W11 Decisions") over REST by param NAME. Never triggers.
Every clip: the skull in slot A, Spin Y 20. Clip name = what it shows.
  C1 Trail Echoes 4, every other Copies control at its (new) default            LoadEhr
  C2 Ghosts 4, every other Copies control at its (new) default                  LoadEhr
  C3-C6 Light C, one clip per C Auto Place option; the C dials are set first to a visibly different aim (Free, yaw 40, cone 60, Behind
        Depth 1.2, not anchored), then C Enable; a chain skull -> Damaged Helmet (Ping-Pong, Hold 3 s) shows what each option does per model
                                                                                LoadEhr
  C7 Cut Window with Proj Enable on (a DXV video): the window shows Slot B (the SciFi helmet), not the video  LoadEhr Wow
usage: tools/arena_w11.py <layer>            every clip
       tools/arena_w11.py <layer> <col>      one column
       tools/arena_w11.py get <layer> <col>  read back the dials this script sets"""
import os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import arena_w5 as A  # noqa: E402  (req / put: REST by param name)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DAMAGED = os.path.join(ROOT, 'test_models', 'DamagedHelmet', 'DamagedHelmet.glb')
DXV = os.path.join(ROOT, 'test_media', 'gen', 'w4', 'code_dxv_1080.mov')
BASE = [('Model Source', 'Model File'), ('Model A', A.SKULL), ('Model B', ''), ('Model C', ''), ('Model D', ''), ('Spin Y', 20)]
AIM = [('C Aim Mode', 'Free'), ('C Aim Yaw', 40), ('C Cone Angle', 60), ('C Behind Depth', 1.2), ('C Anchor To Mask', False)]
CHAIN = [('Model B', DAMAGED), ('Transition Play', 'Ping-Pong'), ('Transition Time', 4), ('Hold Time', 3), ('Haze', 0.5)]


def lightc(option):
    return CHAIN + [('C Auto Place', option)] + AIM + [('C Enable', True)]


CLIPS = {
    1: ('Trail Echoes 4 (new defaults)', [('Trail Echoes', 4)]),
    2: ('Ghosts 4 (new defaults)', [('Ghosts', 4)]),
    3: ('C Auto Place Off', lightc('Off')),
    4: ('C Auto Place Once Per Model', lightc('Once Per Model')),
    5: ('C Auto Place Position Only', lightc('Position Only')),
    6: ('C Auto Place Every Model Swap', lightc('Every Model Swap')),
    7: ('Cut Window Wow Slot World', [('Model B', A.HELMET), ('Rotate Y', 10), ('Cut Look', 'Window'), ('Proj File', DXV), ('Proj Enable', True)]),
}


def dial(layer, col):
    c = A.req('GET', '/composition/layers/%d/clips/%d' % (layer, col))
    name, dials = CLIPS[col]
    sp = c['video']['sourceparams']
    for k, v in BASE + dials:
        A.put(sp, k, v)
    A.req('PUT', '/composition/layers/%d/clips/%d' % (layer, col), {'name': {'value': name}})
    print('C%d %s: %d dials' % (col, name, len(BASE + dials)))


def get(layer, col):
    c = A.req('GET', '/composition/layers/%d/clips/%d' % (layer, col))
    sp = c['video']['sourceparams']
    for k, _ in BASE + CLIPS[col][1]:
        p = sp.get(k)
        print('  %-18s %s' % (k, 'MISSING' if p is None else '%s = %s' % (p['valuetype'], p.get('value'))))
    print('source params in this clip: %d' % len(sp))


if __name__ == '__main__':
    if sys.argv[1] == 'get':
        get(int(sys.argv[2]), int(sys.argv[3]))
    else:
        layer = int(sys.argv[1])
        for col in ([int(sys.argv[2])] if len(sys.argv) > 2 else sorted(CLIPS)):
            dial(layer, col)
