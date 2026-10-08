#!/usr/bin/env python3
"""LoadEhr W8: dial the W8 clips (layer "LoadEhr W8") over REST by param NAME. Never triggers a clip.
usage: tools/arena_w8.py <layer>          every clip below (columns 1-4)
       tools/arena_w8.py <layer> <col>    one column
The HQ colour comparison (DXVKit 1.0.0, chroma midpoint 127.5, as W3 C8 / C9): each clip's SOURCE is the DXV file, LoadEhr Wow FX on
it, same dials. 1 / 3 = Proj Source File (DXVKit decodes the file in the plugin), 2 / 4 = Proj Source Input (Arena's own DXV decode
feeds the input). 1 / 2 = YCG6, 3 / 4 = YG10 (the Alley testsrc2 vectors in the DXVKit submodule: saturated bars, where the old
midpoint 128 and the missing R / B folding showed)."""
import os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import arena_w3 as A  # noqa: E402  (REST helpers, the W3 projector dials)

VEC = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'third_party', 'dxvkit', 'test_vectors', 'streams', 'resolume')
YCG6 = os.path.join(VEC, 'resolume-alley_ycg6_testsrc2_1920x1080.mov')
YG10 = os.path.join(VEC, 'resolume-alley_yg10_testsrc2_1920x1080.mov')
FILES = {1: YCG6, 2: YCG6, 3: YG10, 4: YG10}
CLIPS = {
    1: A.PROJ + [('Proj Source', 'File'), ('Proj File', YCG6), ('Input Use', 'Off')],
    2: A.PROJ + [('Proj Source', 'Input'), ('Input Use', 'Off')],
    3: A.PROJ + [('Proj Source', 'File'), ('Proj File', YG10), ('Input Use', 'Off')],
    4: A.PROJ + [('Proj Source', 'Input'), ('Input Use', 'Off')],
}


def dial(layer, col):
    c = A.req('GET', '/composition/layers/%d/clips/%d' % (layer, col))
    sp = next(e['params'] for e in c['video']['effects'] if e['name'].startswith('LoadEhr Wow FX'))
    for k, v in CLIPS[col]:
        A.put(sp, k, v)
    print('C%d %s: %d dials' % (col, c['name']['value'], len(CLIPS[col])))


if __name__ == '__main__':
    layer = int(sys.argv[1])
    for col in ([int(sys.argv[2])] if len(sys.argv) > 2 else sorted(CLIPS)):
        dial(layer, col)
