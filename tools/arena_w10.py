#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""LoadEhr W10: dial the Copies clips (layer "LoadEhr W10 Copies", LoadEhr source clips) over REST by param NAME. Never triggers.
One clip per Copies control after LOD Bias (the Trail colour quartet = one clip): the skull moving (Spin Y + Wobble), the control's
gates open as docs/param_hierarchy.md says, the control itself pushed to a clearly visible value. Clip name = control name.
usage: tools/arena_w10.py <layer>            every clip (columns 1..18)
       tools/arena_w10.py <layer> <col>      one column
       tools/arena_w10.py get <layer> <col>  read the Copies group back (host slot -> name check)"""
import os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import arena_w5 as A  # noqa: E402  (req / put: REST by param name)

MOVE = [('Model Source', 'Model File'), ('Model A', A.SKULL), ('Model B', ''), ('Model C', ''), ('Model D', ''), ('Spin Y', 20),
        ('Wobble Amount', 1), ('Key Intensity', 2.5)]
ECHO = [('Trail Echoes', 7), ('Trail Offset X', 0.04)]  # the echoes' gates: on, and shifted out from under the live mask
GHOST = [('Ghosts', 8), ('Ghost Spacing', 10)]          # the ghosts' gates: on, far enough apart to leave the live silhouette
CLIPS = {
    1: ('Trail Echoes', [('Trail Offset X', 0.04), ('Trail Echoes', 7)]),
    2: ('Trail Delay', ECHO + [('Trail Delay', 20)]),
    3: ('Trail Decay', ECHO + [('Trail Decay', 1)]),
    4: ('Trail Offset X', [('Trail Echoes', 5), ('Trail Offset X', 0.08)]),
    5: ('Trail Offset Y', [('Trail Echoes', 5), ('Trail Offset Y', 0.06)]),
    6: ('Ghosts', [('Ghost Spacing', 10), ('Ghosts', 8)]),
    7: ('Ghost Spacing', [('Ghosts', 6), ('Ghost Spacing', 30)]),
    8: ('Ghost Fade', GHOST + [('Ghost Fade', 1)]),
    9: ('Trail Smear', ECHO + [('Trail Delay', 8), ('Trail Smear', 1)]),
    10: ('Trail Blur', ECHO + [('Trail Blur', 2)]),
    11: ('Trail Dust', ECHO + [('Trail Dust', 1)]),
    12: ('Trail Grain', ECHO + [('Trail Dust', 0.8), ('Trail Grain', 12)]),
    13: ('Trail Drift', ECHO + [('Trail Dust', 0.8), ('Trail Drift', 1.5)]),
    14: ('Trail Drift Dir', ECHO + [('Trail Dust', 0.8), ('Trail Drift', 1), ('Trail Drift Dir', -90)]),
    15: ('Trail Fade Curve', ECHO + [('Trail Fade Curve', 'Long Tail')]),
    16: ('Trail Glow', ECHO + [('Trail Glow', 5)]),
    17: ('Trail Color', ECHO + [('Trail Color Mix', 0.8), ('Trail Color', (0.0, 1, 1, 1))]),  # one picker: hue, sat, bri, alpha
    18: ('Trail Color Mix', ECHO + [('Trail Color Mix', 1)]),
}
COPIES = ['Clone Layout', 'Clone Count', 'Clone Spacing', 'Clone Spin Step', 'Clone Hue Spread', 'Clone Orbit', 'Clone Scale', 'Tunnel Fade',
          'Keep Original', 'LOD Bias', 'Trail Echoes', 'Trail Delay', 'Trail Decay', 'Trail Offset X', 'Trail Offset Y', 'Ghosts', 'Ghost Spacing',
          'Ghost Fade', 'Trail Smear', 'Trail Blur', 'Trail Dust', 'Trail Grain', 'Trail Drift', 'Trail Drift Dir', 'Trail Fade Curve', 'Trail Glow',
          'Trail Color', 'Trail Color Mix']


def dial(layer, col):
    c = A.req('GET', '/composition/layers/%d/clips/%d' % (layer, col))
    name, dials = CLIPS[col]
    sp = c['video']['sourceparams']
    for k, v in MOVE + dials:
        A.put(sp, k, v)
    A.req('PUT', '/composition/layers/%d/clips/%d' % (layer, col), {'name': {'value': name}})
    print('C%d %s: %d dials' % (col, name, len(MOVE + dials)))


def get(layer, col):
    c = A.req('GET', '/composition/layers/%d/clips/%d' % (layer, col))
    sp = c['video']['sourceparams']
    for n in COPIES:
        p = sp.get(n)
        print('  %-18s %s' % (n, 'MISSING' if p is None else '%s = %s (id %s)' % (p['valuetype'], p.get('value'), p['id'])))
    print('source params in this clip: %d' % len(sp))


if __name__ == '__main__':
    if sys.argv[1] == 'get':
        get(int(sys.argv[2]), int(sys.argv[3]))
    else:
        layer = int(sys.argv[1])
        for col in ([int(sys.argv[2])] if len(sys.argv) > 2 else sorted(CLIPS)):
            dial(layer, col)
