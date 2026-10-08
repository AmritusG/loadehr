#!/usr/bin/env python3
"""LoadEhr W7: dial the W7 clips (LoadEhr Wow source clips, layer "LoadEhr W7") over REST by param NAME. Never triggers.
usage: tools/arena_w7.py <layer> [col]     every clip: Ping-Pong, 6 s a hop, Hold 1.5 s, soft (Spin Y 3, Key 0.8)
       tools/arena_w7.py get <layer> <col> [name ...]"""
import os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import arena_w6 as A  # noqa: E402

M = A.M
HOP = A.SOFT + [('Model Source', 'Model File'), ('Transition Play', 'Ping-Pong'), ('Transition Time', 6), ('Hold Time', 1.5), ('Chain Loop', False),
                ('Style Order', 'Fixed'), ('Trans Ease', 1), ('Trans Stagger', 0.85), ('Trans Spread', 0.6), ('Trans Sparkle', 0),
                ('Model C', ''), ('Model D', '')]


def hop(a, b, style, extra=()):
    return HOP + [('Model A', M[a]), ('Model B', M[b]), ('Transition Style', style)] + list(extra)


CLIPS = {
    1: ('W7 Twist Default', hop('ram', 'crown', 'Twist')),
    2: ('W7 Twist Top End', hop('ram', 'crown', 'Twist', [('Trans Spread', 4), ('Trans Stagger', 1)])),
    3: ('W7 Melt Default', hop('ganesha', 'skull', 'Melt')),
    4: ('W7 Melt Top End', hop('ganesha', 'skull', 'Melt', [('Trans Spread', 4), ('Drip', 3)])),
    5: ('W7 Lava Skull Crown', hop('skull', 'crown', 'Lava')),
    6: ('W7 Lava Helmet', hop('helmet', 'skull', 'Lava')),
    7: ('W7 Sparkle Points', hop('skull', 'helmet', 'Points', [('Trans Sparkle', 3)] + A.CYAN + [('Wire Glow', 1.5), ('Wire Thickness', 1.2)])),
    8: ('W7 Sparkle Spin Swap', hop('nefertiti', 'crown', 'Spin Swap', [('Trans Sparkle', 3)])),
    9: ('W7 Sparkle Dissolve', hop('anubis', 'skull', 'Dissolve', [('Trans Sparkle', 3)])),
    10: ('W7 Sparkle 10', hop('skull', 'helmet', 'Twist', [('Trans Sparkle', 10)])),
    11: ('W7 Cycle 21 Sparkle 2', A.SOFT + [('Model Source', 'Model File'), ('Model A', M['skull']), ('Model B', M['helmet']), ('Model C', M['crown']),
                                         ('Model D', M['nefertiti']), ('Chain Loop', True), ('Transition Play', 'Loop'), ('Style Order', 'Cycle'),
                                         ('Transition Style', 'Points'), ('Transition Time', 6), ('Hold Time', 1.5), ('Trans Sparkle', 2)] + A.CYAN
                                         + [('Wire Glow', 1)]),
}
assert not any('arnival' in str(v) for _, d in CLIPS.values() for _, v in d)  # HARD RULE 3

if __name__ == '__main__':
    if sys.argv[1] == 'get':
        sp = A.req('GET', '/composition/layers/%s/clips/%s' % (sys.argv[2], sys.argv[3]))['video']['sourceparams']
        for n in sys.argv[4:] or sorted(sp):
            print('%s = %r' % (n, sp.get(n, {}).get('value', '<missing>')))
        sys.exit(0)
    layer = int(sys.argv[1])
    for col in ([int(sys.argv[2])] if len(sys.argv) > 2 else sorted(CLIPS)):
        A.dial(CLIPS, layer, col)
