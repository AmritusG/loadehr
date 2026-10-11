#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""LoadEhr W12: dial the Light Glide clips (layer "LoadEhr W12 Light Glide") over REST by param NAME. Never triggers.
Both clips: skull (A) -> Damaged Helmet (B) Ping-Pong (Transition Time 6, Hold 1.5), haze 0.25 without smoke, seen from the side
(Orbit Yaw 60) so the beams show; the C dials are set first to an off aim (At Camera, cone 70, Behind Depth 1.5, not anchored), then
C Auto Place, then C Enable (the finder places them).
  C1 Every Model Swap: the lights re-fit per model and glide (cone, depth, openings arrive together)   LoadEhr
  C2 Once Per Model: placed once for the skull, the openings glide, cone / depth stay                   LoadEhr
usage: tools/arena_w12.py <layer>            every clip
       tools/arena_w12.py <layer> <col>      one column
       tools/arena_w12.py get <layer> <col>  read back the dials this script sets"""
import os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import arena_w5 as A  # noqa: E402  (req / put: REST by param name)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DAMAGED = os.path.join(ROOT, 'test_models', 'DamagedHelmet', 'DamagedHelmet.glb')
BASE = [('Model Source', 'Model File'), ('Model A', A.SKULL), ('Model B', DAMAGED), ('Model C', ''), ('Model D', ''),
        ('Haze', 0.25), ('Smoke Amount', 0), ('Orbit Yaw', 60), ('Transition Play', 'Ping-Pong'), ('Transition Time', 6), ('Hold Time', 1.5),
        ('C Aim Mode', 'At Camera'), ('C Cone Angle', 70), ('C Behind Depth', 1.5), ('C Anchor To Mask', False)]
CLIPS = {
    1: ('W12 C1 Every Model Swap (glide)', [('C Auto Place', 'Every Model Swap'), ('C Enable', True)]),
    2: ('W12 C2 Once Per Model (compare)', [('C Auto Place', 'Once Per Model'), ('C Enable', True)]),
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
    for k, _ in BASE + CLIPS[col][1] + [('C Left Pos X', 0), ('C Placed For', 0)]:
        p = sp.get(k)
        print('  %-18s %s' % (k, 'MISSING' if p is None else '%s = %s' % (p['valuetype'], p.get('value'))))


if __name__ == '__main__':
    a = sys.argv[1:]
    if len(a) == 3 and a[0] == 'get':
        get(int(a[1]), int(a[2]))
    elif len(a) in (1, 2):
        for col in ([int(a[1])] if len(a) == 2 else sorted(CLIPS)):
            dial(int(a[0]), col)
    else:
        print(__doc__)
        sys.exit(1)
