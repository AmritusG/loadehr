#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""LoadEhr W11: Amrit's W10 decisions + the W9 prune, proven in the harness.

usage: tools/w11_decisions.py defaults <out> <ref build>   Stage 1: Trail Echoes / Ghosts raised ALONE (every other Copies control at its
                                                          default), changed px vs Echoes / Ghosts 0 against the noise, both builds
       tools/w11_decisions.py lightc <out>                Stage 3: C Auto Place, per option: C dials set first, the eye finder lands, the
                                                          C params read back (+ a hop to Model B; + positions set by hand)
       tools/w11_decisions.py window <out> <ref build>    Stage 4: the Cut Window. LoadEhr (Slot B world) vs the reference build; Wow with
                                                          Proj Enable on (aimed away: lights nothing) / off (the video world is gone: on = off) in both builds
"""
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import w5_wire as W  # noqa: E402

ROOT = W.ROOT
BUILD = os.environ.get('W11_BUILD', os.path.join(ROOT, 'build'))  # the build under test
DXV = os.path.join(ROOT, 'test_media', 'gen', 'w4', 'code_dxv_1080.mov')
CMP = re.compile(r'^ref_cmp (\S+) mean ([\d.]+) max (\d+) changed (\d+) cover (\d+) (\d+) of (\d+)')
PIN = ['hosttime 100', 'advance 0 zero']
DAMAGED = os.path.join(ROOT, 'test_models', 'DamagedHelmet', 'DamagedHelmet.glb')  # a second model the eye finder finds eyes in


def run(build, lines, out, tag):
    sp = os.path.join(out, tag + '.txt')
    open(sp, 'w').write('\n'.join(lines) + '\n')
    r = subprocess.run([os.path.join(build, 'loadehr_harness'), sp], capture_output=True, text=True, cwd=ROOT)
    return r.stdout + r.stderr


def scene(build, bundle, size=(480, 270)):
    return ['bundle %s/%s.bundle' % (build, bundle), 'size %d %d' % size, 'instance', 'text Model_A ' + W.SKULL, 'wait_loaded 60']


# ---------------------------------------------------------------- Stage 1
def defaults(out, ref):
    os.makedirs(out, exist_ok=True)
    rows = ['| build | scene | control raised alone | changed px | noise px | coverage (base -> raised) | verdict |', '|---|---|---|---|---|---|---|']
    scenes = {'still': [], 'moving': ['set Spin Y 30', 'set Wobble Amount 1', 'set Pos X 0.4']}
    for build, bn in ((BUILD, 'W11'), (ref, 'W10')):
        for sc, mot in scenes.items():
            L = scene(build, 'LoadEhr') + ['save_state']
            for ctl, val, settle in (('Trail Echoes', 4, 123), ('Ghosts', 4, 263)):
                def one(action, cmp):
                    return ['restore_state', 'frames 1'] + PIN + mot + ['frames 30'] + action + ['frames %d' % settle] + cmp
                L += one([], [])  # a warm-up run: the instance's first run differs from the next (9575 px moving, measured)
                L += one([], ['ref_save base'])
                L += one([], ['ref_cmp %s|noise base' % ctl.replace(' ', '_')])
                L += one(['set %s %g' % (ctl, val)], ['ref_cmp %s|on base' % ctl.replace(' ', '_'), 'composite arena',
                                                      'shot %s/%s_%s_%s.png' % (out, bn, sc, ctl.replace(' ', '_'))])
            got = {}
            for line in run(build, L, out, 'defaults_%s_%s' % (bn, sc)).splitlines():
                m = CMP.match(line)
                if m:
                    got[m.group(1)] = (int(m.group(4)), int(m.group(5)), int(m.group(6)))
            for ctl in ('Trail Echoes', 'Ghosts'):
                on, nz = got.get(ctl.replace(' ', '_') + '|on'), got.get(ctl.replace(' ', '_') + '|noise')
                if not on or not nz:
                    rows.append('| %s | %s | %s | ? | ? | ? | harness failed |' % (bn, sc, ctl))
                    continue
                vis = on[0] >= 130 and on[0] > 3 * nz[0]
                rows.append('| %s | %s | %s 4 | %d | %d | %d -> %d | %s |' % (bn, sc, ctl, on[0], nz[0], on[1], on[2], 'VISIBLE' if vis else 'not visible'))
    print('\n'.join(rows))
    open(os.path.join(out, 'defaults.md'), 'w').write('\n'.join(rows) + '\n')


# ---------------------------------------------------------------- Stage 3
CPARAMS = ['C Left Pos X', 'C Left Pos Y', 'C Left Pos Z', 'C Right Pos X', 'C Aim Mode', 'C Cone Angle', 'C Behind Depth', 'C Anchor To Mask', 'C Mirror X']
PRESET = [('C Aim Mode', 1), ('C Cone Angle', 70), ('C Behind Depth', 1.5), ('C Anchor To Mask', 0)]  # a visibly different aim, set first
HAND = [('C Left Pos X', -1.2), ('C Left Pos Y', 0.9)]  # positions moved by hand before any placement
MODES = ['Off', 'Once Per Model', 'Position Only', 'Every Model Swap']


def gets(lines):
    v = {}
    for l in lines.splitlines():
        m = re.match(r'^get (.+?) = (\S+)', l)
        if m:
            v.setdefault(m.group(1), []).append(m.group(2))
    return v


def lightc(out):
    os.makedirs(out, exist_ok=True)
    build = BUILD
    rows = ['| C Auto Place | case | ' + ' | '.join(CPARAMS) + ' |', '|---|---|' + '---|' * len(CPARAMS)]
    for mi, mn in enumerate(MODES):
        for case, pre in (('dials preset', PRESET), ('dials + positions by hand', PRESET + HAND)):
            L = scene(build, 'LoadEhr', (640, 360)) + ['set C Auto Place %d' % mi] + ['set %s %g' % kv for kv in pre]
            L += ['get %s' % p for p in CPARAMS]                                   # 1: as set
            L += ['set C Enable 1', 'realtime 2.5'] + ['get %s' % p for p in CPARAMS]  # 2: the finder landed
            L += ['composite arena', 'shot %s/lightc_%d_%s.png' % (out, mi, 'hand' if len(pre) > len(PRESET) else 'preset')]
            # 3: a second model, the chain resting on it (Transition 1: Model B is on show = the dominant model)
            L += ['mark', 'text Model_B ' + DAMAGED, 'wait_slots 1 60', 'set Transition 1', 'realtime 2.5'] + ['get %s' % p for p in CPARAMS]
            v = gets(run(build, L, out, 'lightc_%d_%d' % (mi, len(pre))))
            for k, stage in enumerate(('set', 'after the finder (skull)', 'after a hop to Model B (Damaged Helmet)')):
                cells = [v.get(p, ['?'] * 3)[k] if len(v.get(p, [])) > k else '?' for p in CPARAMS]
                rows.append('| %s | %s: %s | %s |' % (mn, case, stage, ' | '.join(cells)))
    print('\n'.join(rows))
    open(os.path.join(out, 'lightc.md'), 'w').write('\n'.join(rows) + '\n')
    # the Window with C Auto Place Off: the finder's own openings, no C dial written; vs Once Per Model (dials placed at the same eyes)
    shots = []
    for mi in (0, 1):
        L = scene(build, 'LoadEhr', (640, 360)) + ['mark', 'text Model_B ' + W.HELMET, 'wait_slots 1 60', 'set Rotate Y 10',
                                                    'set C Auto Place %d' % mi, 'set Cut Look 3', 'realtime 2.5'] + PIN + \
            ['frames 3', 'composite arena', 'shot %s/window_auto%d.png' % (out, mi)] + ['get %s' % p for p in CPARAMS[:3]]
        shots.append(gets(run(build, L, out, 'window_auto%d' % mi)))
    so = run(build, ['compare %s/window_auto0.png %s/window_auto1.png' % (out, out)], out, 'window_auto_cmp')
    m = re.search(r'mean ([\d.]+) max (\d+) pixels>2 (\d+)', so)
    txt = ['', 'Window (Cut Look 3, skull A + helmet B, C Enable off), C Auto Place Off vs Once Per Model: %s' % (m.group(0) if m else so.strip()),
           'C Left Pos X/Y/Z after: Off %s, Once %s' % ([shots[0].get(p, ['?'])[0] for p in CPARAMS[:3]], [shots[1].get(p, ['?'])[0] for p in CPARAMS[:3]])]
    print('\n'.join(txt))
    open(os.path.join(out, 'lightc.md'), 'a').write('\n'.join(txt) + '\n')


# ---------------------------------------------------------------- Stage 4
def window(out, ref):
    os.makedirs(out, exist_ok=True)
    rows = ['| case | compare | result |', '|---|---|---|']
    for build, bn in ((BUILD, 'W11'), (ref, 'W10')):
        base = lambda bundle: scene(build, bundle, (640, 360)) + ['mark', 'text Model_B ' + W.HELMET, 'wait_slots 1 60', 'set Rotate Y 10', 'set Cut Look 3']
        L = base('LoadEhr') + ['realtime 2'] + PIN + ['frames 3', 'composite arena', 'shot %s/%s_classic_window.png' % (out, bn)]
        run(build, L, out, '%s_classic' % bn)
        for proj in (0, 1):
            L = base('LoadEhrWow') + ['text Proj_File ' + DXV, 'set Proj Enable %d' % proj, 'set Proj Beam 0', 'set Proj Surface 1',
                                      'set Proj Aim Mode 1', 'set Proj Aim Yaw 180',  # on, aimed away: it lights nothing
                                      'realtime 2.5'] + PIN + ['frames 3', 'composite arena', 'shot %s/%s_wow_window_proj%d.png' % (out, bn, proj)]
            run(build, L, out, '%s_wow_proj%d' % (bn, proj))
    for a, b, what in (('W11_classic_window', 'W10_classic_window', 'LoadEhr Window (Slot B world): W11 vs W10'),
                       ('W11_wow_window_proj1', 'W11_wow_window_proj0', 'Wow Window, Proj Enable on (aimed away) vs off: W11'),
                       ('W10_wow_window_proj1', 'W10_wow_window_proj0', 'Wow Window, Proj Enable on (aimed away) vs off: W10'),
                       ('W11_wow_window_proj0', 'W10_wow_window_proj0', 'Wow Window, Proj Enable off: W11 vs W10')):
        so = run(BUILD, ['compare %s/%s.png %s/%s.png' % (out, a, out, b)], out, 'cmp_' + a)
        m = re.search(r'mean ([\d.]+) max (\d+) pixels>2 (\d+) of (\d+)', so)
        rows.append('| %s | %s.png vs %s.png | %s |' % (what, a, b, m.group(0) if m else so.strip()[:120]))
    print('\n'.join(rows))
    open(os.path.join(out, 'window.md'), 'w').write('\n'.join(rows) + '\n')


if __name__ == '__main__':
    a = sys.argv[1:]
    if not a:
        print(__doc__)
    elif a[0] == 'defaults':
        defaults(a[1], a[2])
    elif a[0] == 'lightc':
        lightc(a[1])
    elif a[0] == 'window':
        window(a[1], a[2])
