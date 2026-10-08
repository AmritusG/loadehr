#!/usr/bin/env python3
"""W14: fast-forward every integrated clock to just before a wrap point at its maximum speed and measure the
frame-to-frame change across it. usage (repo root): tools/w14_wraps.py <before|after|times> <out dir> [name prefix...]
Each test: fresh instance, the feature alone at max speed, `advance` to (wrap - lead), then `diffs N` (1/60 s
frames). Printed: the largest per-frame change in the window vs the median (normal motion)."""
import os, re, subprocess, sys, statistics

MODEL = os.path.abspath('test_models/gen/mask/mask.obj')  # run from the repo root (tools/gen_test_models.py makes it)
GEN = os.path.abspath('test_models/gen')
# LoadEhr W1: the morph chain's other slots (B, C, D)
SLOTS = ['text Model_B %s/knot/knot.obj' % GEN, 'text Model_C %s/crystal/crystal.obj' % GEN, 'text Model_D %s/star/star.obj' % GEN]
LOWPOLY = [('Low Poly', 1), ('Low Poly Size', 0.3)]
VOL = [('A Enable', 1), ('A Pos Z', 1.5), ('A Aim Mode', 1)]
TEXT = [('Model Source', 1)]  # W21: the default text "LOADEHR"
PI = 3.14159265358979
SP = 200 * PI  # Smooth1D period
# name, max speed, params, {mode: (wrap time s, lead s, frames)}, extra commands before the window.
# before = the W13 code, whose float clocks drift from the nominal wrap time, so the window is wide and the spike is
# found by search; after = W14 (double clocks from zero): the wrap falls exactly inside diff index 60*lead - 1.
TESTS = [
    ('Spin Y', '30 deg/s', [('Spin Y', 30)], {'before': (12, 1, 120), 'after': (12, 1, 120)}),
    ('Wobble', 'speed 5 (10 rad/s)', [('Wobble Amount', 2), ('Wobble Speed', 5)], {'after': (5 * SP / 10, 1, 120)}),
    ('Beat Shake', 'shake 4, 120 bpm', [('Beat Shake', 4), ('Shake Decay', 2)], {'after': (SP, 1, 120)}),
    ('Clone Orbit (tunnel)', '30 deg/s', [('Clone Layout', 3), ('Clone Orbit', 30)], {'before': (12000, 1, 120), 'after': (120, 1, 120)}),
    # control: 5 turns is not a wrap, but a tunnel ring recycles (front -> back) on every whole turn by design
    ('Clone tunnel @5 turns', '30 deg/s (no wrap)', [('Clone Layout', 3), ('Clone Orbit', 30)], {'after': (60, 1, 120)}),
    ('Clone Orbit (carousel)', '30 deg/s', [('Clone Layout', 4), ('Clone Orbit', 30)], {'after': (120, 1, 120)}),
    ('Spot sweep (Pan)', '15 sweeps/min', [('Spot Count', 4), ('Spot Speed', 15), ('Haze', 0.4)], {'after': (4, 1, 120)}),
    ('Spot sweep (Random)', '15 sweeps/min', [('Spot Count', 4), ('Spot Pattern', 4), ('Spot Speed', 15), ('Haze', 0.4)],
     {'before': (4000, 8, 720), 'after': (2 * SP / 0.25, 1, 120)}),
    ('LowPoly Spin X', '30 deg/s', LOWPOLY + [('LowPoly Spin X', 30)], {'before': (12, 1, 120), 'after': (12, 1, 120)}),
    ('Tumble steady (Separate)', '30 deg/s xyz, sway 0.3', LOWPOLY + [('Facet Tumble', 2), ('Tumble X', 30), ('Tumble Y', 30), ('Tumble Z', 30), ('Tumble Sway', 0.3), ('Tumble Random', 1)],
     {'before': (768, 1, 120), 'after': (360 / 21, 1, 120)}),
    ('Tumble sway (Separate)', '30 deg/s xyz, sway 0.3', LOWPOLY + [('Facet Tumble', 2), ('Tumble X', 30), ('Tumble Y', 30), ('Tumble Z', 30), ('Tumble Sway', 0.3), ('Tumble Random', 1)],
     {'after': (12, 1, 120)}),
    ('Tumble steady (Together)', '30 deg/s x, sway 0.3', LOWPOLY + [('Facet Tumble', 1), ('Tumble X', 30), ('Tumble Sway', 0.3)],
     {'before': (768, 1, 120), 'after': (360 / 21, 1, 120)}),
    ('Tumble sway (Together)', '30 deg/s x, sway 0.3', LOWPOLY + [('Facet Tumble', 1), ('Tumble X', 30), ('Tumble Sway', 0.3)], {'after': (12, 1, 120)}),
    ('Hue Drift A', '0.7 turns/s', VOL + [('A Hue Drift', 0.7), ('A Sat', 1)], {'before': (1 / 0.7, 0.5, 60), 'after': (1 / 0.7, 0.5, 60)}),
    # max speed at a visible scale (at Smoke Scale 10 the grain decorrelates every frame, so no jump can show)
    ('Smoke drift', 'scale 1.5 speed 4', VOL + [('A Aim Mode', 0), ('Smoke Amount', 4), ('Smoke Scale', 1.5), ('Smoke Speed', 4), ('Smoke Rise', 0)],
     {'before': (64 / 1.26, 3, 360), 'after': (100 / 1.26, 1, 120)}),
    ('Motes', 'motes 4', VOL + [('Motes', 4)], {'before': (3600, 4, 480), 'after': (3600, 1, 120)}),
    ('Glitter Twinkle', '1 /s', [('Glitter', 10), ('Glitter Twinkle', 1)], {'before': (3600, 4, 480), 'after': (1000, 1, 120)}),
    ('Pattern Stripes', 'speed 4', [('Pattern', 1), ('Pattern Speed', 4)], {'after': (1000 / 4, 1, 120)}),
    ('Pattern Noise', 'speed 4', [('Pattern', 3), ('Pattern Speed', 4)], {'before': (3600, 4, 480), 'after': (1000 / 4, 1, 120)}),
    ('Pattern Noise (fade mid)', 'speed 4', [('Pattern', 3), ('Pattern Speed', 4)], {'after': (975 / 4, 1, 120)}),
    ('Pattern Cells', 'speed 4', [('Pattern', 2), ('Pattern Speed', 4)], {'before': (3600, 4, 480), 'after': (1000 / 4, 1, 120)}),
    ('Oil Film swirl', 'swirl 2 (0.07/s)', [('Oil Film', 2), ('Film Swirl', 2)], {'before': (3600, 4, 480), 'after': (256 / 0.07, 1, 120)}),
    ('Chrome Ripple', 'ripple 3', [('Liquid Chrome', 1), ('Chrome Ripple', 3)], {'before': (3600, 4, 480), 'after': (64 / 0.3, 1, 120)}),
    ('Hologram scan', 'holo 1', [('Hologram', 1), ('Holo Flicker', 0)], {'before': (3600, 4, 480), 'after': (PI / 2, 0.5, 60)}),
    ('Holo flicker', 'flicker 2', [('Hologram', 1), ('Holo Flicker', 2)], {'after': (3600, 1, 120)}),
    ('Slice Wave', 'wave 4', [('Slice Wave', 4), ('Slice Count', 12)], {'before': (3600, 4, 480), 'after': (PI, 1, 120)}),
    ('Ripple', 'speed 20', [('Ripple', 0.2), ('Ripple Speed', 20)], {'before': (1000 * 2 * PI / 20, 1, 120), 'after': (2 * PI / 20, 0.2, 60)}),
    ('Contour scroll', 'speed 4', [('Contours', 5), ('Contour Speed', 4)], {'before': (250, 1, 120), 'after': (0.25, 0.1, 60)}),
    ('Dust wind', 'wind 6', [('Dust Wind', 6), ('Dust Hold', 30)], {'after': (256, 1, 120)}),
    # W15: the Trail Dust grain drifts in cells (the grain hash repeats every 256): 0.1 screen heights/s at 2.5 px grain
    # = 43.2 cells/s, wrap at 256 / 43.2 s (a visible speed; at 2 the grain decorrelates every frame)
    ('Trail Dust grain', 'drift 0.1 (43.2 cells/s)', [('Trail Echoes', 7), ('Trail Offset X', 0.02), ('Trail Smear', 1), ('Trail Dust', 1), ('Trail Drift', 0.1)],
     {'after': (256 / 43.2, 1, 120)}),
    ('Trail Dust grain @max', 'drift 2 (864 cells/s)', [('Trail Echoes', 7), ('Trail Offset X', 0.02), ('Trail Dust', 1), ('Trail Drift', 2)],
     {'after': (20 * 256 / 864, 1, 120)}),
    # W15 Drift Return: A drifts to -0.1 turns (hue offset 0.9), then Hue Drift 0 glides it home through the wrap at 1 -> 0;
    # the glide ends (offset exactly 0) 0.4 s = 24 frames after the dial: the frame "at wrap" is the glide's last step
    # W21 letters (typed text): the wave wraps at 1 cycle, the spin at 360 deg (Separate: whole-number factors -2..2).
    # The Type Speed cycle restarting (letters vanish, type on again) is the design, not a wrap.
    ('Letter Wave', '5 cycles/s', TEXT + [('Letter Wave', 2), ('Wave Speed', 5)], {'after': (1 / 5, 0.1, 30)}),
    ('Letter Spin (Separate)', '30 deg/s', TEXT + [('Letter Spin', 30), ('Spin Mode', 1)], {'after': (12, 1, 120)}),
    ('Drift Return A (wrap)', '-0.7 turns/s, return 0.4 s', VOL + [('A Hue Drift', -0.7), ('A Sat', 1)],
     {'after': (0.1 / 0.7, 0, 60)}, ['frames 1', 'set A Hue Drift 0'], 24),
    # W24 Trip (noise smoke, a volume light): palette turns wrap at 1, kaleido / fractal angles at 360, band scroll at 1
    ('Trip Palette Shift', '2 turns/s', VOL + [('Smoke Amount', 3), ('Colour By', 6), ('Palette Shift', 2)], {'after': (0.5, 0.1, 30)}),
    ('Trip Kaleido Spin', '30 deg/s', VOL + [('Smoke Amount', 3), ('Kaleido Smoke', 6), ('Kaleido Spin', 30)], {'after': (12, 1, 120)}),
    ('Trip Fractal Morph', '30 deg/s', VOL + [('Smoke Amount', 3), ('Fractal Fog', 1), ('Fractal Morph', 30)], {'after': (12, 1, 120)}),
    ('Trip Band Scroll', '4 bands/s', VOL + [('Smoke Amount', 3), ('Smoke Contours', 3), ('Band Scroll', 4)], {'after': (0.25, 0.1, 30)}),
    ('Fire flicker light', 'fire speed 2 (W28)', [('Fire', 1), ('Fire Speed', 2), ('Emit Eyes', 4), ('Fire Light', 10), ('Flame Glow', 0)], {'after': (SP / 2, 1, 120)}),
    ('Swarm wander', 'speed 4 (0.6 /s)', [('Swarm Count', 2), ('Swarm Speed', 4), ('Attraction', 0.5)], {'after': (64 / 0.6, 1, 120)}),
    ('Swarm blink', 'twinkle 1', [('Swarm Count', 2), ('Twinkle', 1), ('Swarm Speed', 0)], {'after': (3600, 1, 120)}),
    ('Lava flow', '0.12 /s (W28)', [('Lava Cracks', 1), ('Crack Spread', 1), ('Lava Glow', 6)], {'after': (64 / 0.12, 1, 120)}),
    ('Lava heat haze', '0.5 cycles/s', [('Lava Cracks', 1), ('Crack Spread', 1), ('Heat Haze', 2)], {'after': (2, 0.5, 60)}),
    ('Caustic ripple', '0.5 /s', [('Glass', 1), ('Caustics', 2), ('Caustic Source', 1), ('Caustic Ripple', 1), ('Caustic Floor', 2), ('Shadow Catcher', 0.5)], {'after': (128, 1, 120)}),
    # W28
    ('Liquid wobble', 'speed 2 (1.2 c/s)', [('Liquid Wobble', 1), ('Liquid Speed', 2)], {'after': (1 / 1.2, 0.2, 40)}),
    ('Transition Ping-Pong', 'time 0.5 s', [('Transition Play', 2), ('Transition Time', 0.5)], {'after': (1.0, 0.2, 40)}),
    # LoadEhr W1: the morph chain's clock (4 slots). Loop + Chain Loop wraps at 4 hops (the last hop lands on A); Ping-Pong at 2 x 3
    # hops; with Hold Time each hop is morph + hold. The wrap is a pure model on both sides: no change there but the normal motion.
    ('Chain Loop (4 slots)', 'time 0.5 s / hop', [('Transition Play', 3), ('Chain Loop', 1), ('Transition Time', 0.5), ('Style Order', 1)], {'after': (2.0, 0.2, 40)}),
    ('Chain Loop + Hold', 'time 0.5 s, hold 0.5 s', [('Transition Play', 3), ('Chain Loop', 1), ('Transition Time', 0.5), ('Hold Time', 0.5)], {'after': (4.0, 0.7, 60)}),
    ('Chain Ping-Pong (4 slots)', 'time 0.5 s / hop', [('Transition Play', 2), ('Transition Time', 0.5), ('Style Order', 2)], {'after': (3.0, 0.2, 40)}),
    # LoadEhr W2 Deform: the Noise Warp drift wraps at 256 lattice cells (x 1 : y 0.5 : z 0.25 = whole 64-periods), the Glitch step
    # at 1000 steps (a wrap is one more random step); Ripple's phase is in the row above (2 pi)
    ('Noise Warp drift', '4 cells/s', [('Noise Warp', 1), ('Warp Scale', 2), ('Warp Speed', 4)], {'after': (256 / 4, 1, 120)}),
    ('Glitch step', '30 steps/s', [('Glitch Shift', 0.3), ('Glitch Rate', 30)], {'after': (1000 / 30, 1, 120)}),
    # LoadEhr W6: Point Twinkle's clock runs at 1 twinkle / s and wraps at 1000 (every point's rate is a whole number of twinkles per 1000)
    ('Point Twinkle', '1 twinkle/s, depth 1', [('Wire Mode', 4), ('Point Twinkle', 1), ('Wire Glow', 1)], {'after': (1000, 1, 120)}),
]
# W26 / W28 features live in the Wow bundles only (Resolume's 512-param cap)
# extra commands before the clocks are fast-forwarded (a second model)
SETUP = {'Liquid wobble': ['text Model_B ' + MODEL, 'wait_loaded 60', 'set Transition_Style 4', 'set Transition 0.5', 'frames 30'],
         'Transition Ping-Pong': ['text Model_B ' + MODEL, 'wait_loaded 60', 'frames 1'],
         'Chain Loop (4 slots)': ['mark'] + SLOTS + ['wait_slots 3 60', 'frames 1'],
         'Chain Loop + Hold': ['mark'] + SLOTS + ['wait_slots 3 60', 'frames 1'],
         'Chain Ping-Pong (4 slots)': ['mark'] + SLOTS + ['wait_slots 3 60', 'frames 1']}
WOW = ('Fire', 'Swarm', 'Lava', 'Caustic', 'Liquid', 'Transition')


# `times`: every clock at max speed at once (two scenes), rendered after 1, 13, 26, 60 and 180 minutes of show time:
# the per-frame change must not grow with show time (no precision jitter).
SCENES = [
    ('motion', LOWPOLY + VOL + [('Spin Y', 30), ('Wobble Amount', 2), ('Wobble Speed', 5), ('Beat Shake', 2), ('Spot Count', 4), ('Spot Pattern', 4),
                ('Spot Speed', 15), ('Clone Layout', 4), ('Clone Orbit', 30), ('Clone Count', 4), ('Facet Tumble', 2), ('Tumble X', 30),
                ('Tumble Y', -30), ('Tumble Z', 30), ('Tumble Sway', 0.3), ('LowPoly Spin Y', 30), ('A Hue Drift', 0.7), ('A Sat', 1),
                ('Smoke Amount', 4), ('Smoke Scale', 1.5), ('Smoke Speed', 4), ('Smoke Rise', 2), ('Motes', 4)]),
    # LoadEhr W1: the chain clock over 4 slots (Loop + Chain Loop, every style in turn, a hold): no drift after 180 min either
    ('chain', [('Transition Play', 3), ('Chain Loop', 1), ('Transition Time', 0.5), ('Hold Time', 0.25), ('Style Order', 1), ('Spin Y', 30)]),
    # LoadEhr W6: the same chain in the Wow bundle: Style Order Cycle goes round all 23 styles there (Swarm, Burn, Lava, Smoke included)
    ('chainwow', [('Transition Play', 3), ('Chain Loop', 1), ('Transition Time', 0.5), ('Hold Time', 0.25), ('Style Order', 1), ('Spin Y', 30)]),
    # W6: with Cycle the style moves on every pass (a pass counter), so each checkpoint shows another style; the clock itself = Fixed
    ('chainfixed', [('Transition Play', 3), ('Chain Loop', 1), ('Transition Time', 0.5), ('Hold Time', 0.25), ('Style Order', 0), ('Transition Style', 6)]),
    ('surface', [('Pattern', 3), ('Pattern Speed', 4), ('Glitter', 10), ('Glitter Twinkle', 1), ('Liquid Chrome', 0.6), ('Chrome Ripple', 3),
                 ('Oil Film', 2), ('Film Swirl', 2), ('Contours', 5), ('Contour Speed', 4), ('Ripple', 0.1), ('Ripple Speed', 20),
                 ('Slice Wave', 4), ('Dust Wind', 6)]),
    # LoadEhr W2: the Deform clocks + the Jelly spring (a state, not a phase: it must stay bounded, not drift) over 180 min
    ('deform', [('Spin Y', 30), ('Wobble Amount', 2), ('Wobble Speed', 5), ('Jelly', 2), ('Noise Warp', 1), ('Warp Speed', 4),
                ('Glitch Shift', 0.3), ('Glitch Rate', 6), ('Twist', 2), ('Ripple', 0.05), ('Ripple Speed', 20), ('Squash Beat', 1)]),
]
MINUTES = [1, 13, 26, 60, 180]


def times(out, only=()):
    lines, bundle = ['size 960 540'], None
    scenes = [sc for sc in SCENES if not only or sc[0] in only]
    for scene, params in scenes:
        b = 'LoadEhrWow' if scene in ('chainwow', 'chainfixed') else 'LoadEhr'
        if b != bundle:
            lines, bundle = lines + ['bundle build/%s.bundle' % b], b
        for m in MINUTES:
            lines += ['instance', 'text Model_A ' + MODEL, 'wait_loaded 60'] + ['set %s %g' % (k, v) for k, v in params]
            if scene.startswith('chain'):
                lines += ['mark'] + SLOTS + ['wait_slots 3 60']
            lines += ['advance %d zero' % (m * 60), 'frames 1', 'diffs 60', 'shot %s/times_%s_%03dmin.png' % (out, scene, m), 'delete']
    script = os.path.join(out, 'times.txt')
    open(script, 'w').write('\n'.join(lines) + '\n')
    res = subprocess.run(['./build/loadehr_harness', script], capture_output=True, text=True)
    open(os.path.join(out, 'times.log'), 'w').write(res.stdout + res.stderr)
    diffs = iter([list(map(float, l.split()[1:])) for l in res.stdout.splitlines() if l.startswith('diffs:')])
    print('%-8s %6s | %9s %9s %9s' % ('scene', 'min', 'median', 'p95', 'max'))
    for scene, _ in scenes:
        for m in MINUTES:
            d = sorted(next(diffs))
            print('%-8s %6d | %9.3f %9.3f %9.3f' % (scene, m, statistics.median(d), d[int(0.95 * (len(d) - 1))], d[-1]))


def main():
    if sys.argv[1] == 'times':
        os.makedirs(sys.argv[2], exist_ok=True)
        return times(sys.argv[2], sys.argv[3:])
    mode, out, only = sys.argv[1], sys.argv[2], sys.argv[3:]
    os.makedirs(out, exist_ok=True)
    lines, bundle = ['size 960 540'], None
    run = []
    for name, speed, params, modes, *custom in TESTS:
        if mode not in modes or (only and not any(name.startswith(o) for o in only)):
            continue
        wrap, lead, n = modes[mode]
        run.append((name, speed, wrap, lead, n, custom[1] if len(custom) > 1 else None))
        b = 'LoadEhrWow' if name.startswith(WOW) else 'LoadEhr'
        if b != bundle:
            lines, bundle = lines + ['bundle build/%s.bundle' % b], b
        lines += ['instance', 'text Model_A ' + MODEL, 'wait_loaded 60']
        lines += ['set %s %g' % (k, v) for k, v in params]
        if ('Model Source', 1) in params:
            lines += ['wait_loaded 20']
        lines += SETUP.get(name, [])
        if mode == 'before':
            lines += ['frames 2', 'advance %.6f' % (wrap - lead), 'frames 1']
        else:
            lead += 0.5 / 60  # the wrap falls half-way between two frames
            pre = ['pulse Blow To Dust', 'frames 29'] if name == 'Dust wind' else custom[0] if custom else ['frames 1']
            lines += ['advance %.9f zero' % (wrap - lead - (30 / 60 if name == 'Dust wind' else 0))] + pre
        lines += ['diffs %d' % n, 'shot %s/%s_%s.png' % (out, mode, re.sub('[^a-z0-9]+', '_', name.lower())), 'delete']
    script = os.path.join(out, 'wraps_%s.txt' % mode)
    open(script, 'w').write('\n'.join(lines) + '\n')
    res = subprocess.run(['./build/loadehr_harness', script], capture_output=True, text=True)
    open(os.path.join(out, 'wraps_%s.log' % mode), 'w').write(res.stdout + res.stderr)
    diffs = [list(map(float, l.split()[1:])) for l in res.stdout.splitlines() if l.startswith('diffs:')]
    if mode == 'before':
        print('%-26s %-24s %9s | %8s %8s %6s | %s' % ('clock', 'max speed', 'wrap s', 'max/frm', 'median', 'ratio', 'frame of max (nominal wrap)'))
        for (name, speed, wrap, lead, n, _), d in zip(run, diffs):
            mx, med = max(d), statistics.median(d)
            print('%-26s %-24s %9.2f | %8.3f %8.3f %6.2f | %d (%d)' % (name, speed, wrap, mx, med, mx / med if med > 0 else 0, d.index(mx), round(lead * 60 - 1)))
        return
    # after: the change across the wrap vs the largest change anywhere else in the window (normal motion)
    print('%-26s %-24s %9s | %9s %9s %9s %6s | %s' % ('clock', 'max speed', 'wrap s', 'at wrap', 'max other', 'median', 'ratio', 'verdict'))
    for (name, speed, wrap, lead, n, kk), d in zip(run, diffs):
        k = kk if kk is not None else round(lead * 60) - 1
        other = d[:k] + d[k + 1:]
        mo, med = max(other), statistics.median(other)
        ratio = d[k] / mo if mo > 0 else 0
        print('%-26s %-24s %9.2f | %9.3f %9.3f %9.3f %6.2f | %s' % (name, speed, wrap, d[k], mo, med, ratio, 'ok' if d[k] <= mo * 1.05 + 1e-3 else 'SPIKE'))


main()
