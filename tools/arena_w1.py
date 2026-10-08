#!/usr/bin/env python3
"""LoadEhr W1: dial a look into an Arena clip over REST, by param NAME (never triggers a clip). Recipes are the look books'
dial lists (HSBA quartets become the one Arena colour picker: '<X> Color' = hex, alpha from the quartet's ALPHA).
usage: arena_w1.py <clip id> <look> [maskehr]     look = a parity slug (tools/parity.py list) or one of CHAIN below
       arena_w1.py <clip id> get [name ...]
'maskehr' = the clip holds MaskEhr / MaskEhr Wow (Model A is called Model File there)."""
import colorsys, json, os, sys, urllib.request
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.dont_write_bytecode = True
import w22_smoke as W
import w26_wow as V
import w28_review as F

API = 'http://127.0.0.1:8080/api/v1'
GEN = os.path.join(W.ROOT, 'test_models', 'gen')
# four visually distinct files on this Mac (by path; none from "Not For Shows")
MODELS = [W.MASK, V.SKULL, W.LIB + '/Helmets/Diving Helmet/scene.gltf', W.LIB + '/Stage Icons/Crystal Cluster/scene.gltf']
SOFT = [('Spin Y', 4), ('Key Intensity', 2.5), ('Orbit Yaw', 15)]
# the four W1 morph-chain clips (soft: slow hops, rests between them)
CHAIN = {
    'chain_a_liquid_loop': [('Model A', MODELS[0]), ('Model B', MODELS[1]), ('Model C', MODELS[2]), ('Model D', MODELS[3]),
                            ('Transition Style', 4), ('Liquid Look', 0), ('Liquid Goo', 0.6), ('Liquid Speed', 0.2),
                            ('Chain Loop', 1), ('Transition Play', 3), ('Transition Time', 8), ('Hold Time', 2)] + SOFT,
    'chain_b_cycle_pingpong': [('Model A', MODELS[1]), ('Model B', MODELS[2]), ('Model C', MODELS[3]), ('Style Order', 1),
                               ('Transition Style', 0), ('Transition Play', 2), ('Transition Time', 6), ('Hold Time', 1.5),
                               ('Trans Stagger', 0.85), ('Trans Spread', 0.6)] + SOFT,
    'chain_c_shatter_once': [('Model A', MODELS[0]), ('Model B', MODELS[3]), ('Transition Style', 1), ('Transition Play', 0),
                             ('Transition Time', 6), ('Trans Spread', 0.6)] + SOFT,
    'chain_d_dissolve_slow': [('Model A', MODELS[2]), ('Model B', MODELS[0]), ('Model C', MODELS[3]), ('Model D', MODELS[1]),
                              ('Transition Style', 0), ('Chain Loop', 1), ('Transition Play', 3), ('Transition Time', 12),
                              ('Hold Time', 5)] + SOFT,
}


def req(method, path, body=None):
    r = urllib.request.Request(API + path, method=method, data=None if body is None else json.dumps(body).encode(),
                               headers={'Content-Type': 'application/json'})
    with urllib.request.urlopen(r, timeout=10) as f:
        t = f.read()
        return json.loads(t) if t else None


def recipe(look):
    if look in CHAIN:
        return CHAIN[look], None
    for e in W.EXAMPLES:
        if 'w22_' + W.slug(e) == look:
            return list(e['dials']), e['model']
    for f in F.FIXES:
        e = F.look(f['name'])
        if 'w28_' + V.slug(e) == look:
            return V.recipe_dials(e) + list(e.get('arena', [])), e.get('model')
    raise SystemExit('unknown look ' + look)


def put(sp, name, value):
    p = sp.get(name)
    if p is None:
        print('MISSING', name)
        return
    t = p['valuetype']
    if t == 'ParamChoice':
        val = value if isinstance(value, str) else p['options'][int(round(value))]
    elif t == 'ParamBoolean':
        val = bool(value)
    elif t in ('ParamString', 'ParamFile', 'ParamText', 'ParamColor'):
        val = value
    else:
        val = float(value)
    req('PUT', '/parameter/by-id/%d' % p['id'], {'value': val})
    print('  %s = %r' % (name, val))


def main():
    cid, look = sys.argv[1], sys.argv[2]
    sp = req('GET', '/composition/clips/by-id/%s' % cid)['video']['sourceparams']
    if look == 'get':
        for n in sys.argv[3:] or sp:
            print('%s = %r' % (n, sp.get(n, {}).get('value', '<missing>')))
        return
    maskehr = len(sys.argv) > 3 and sys.argv[3] == 'maskehr'
    dials, model = recipe(look)
    d = dict(dials)
    if model:
        dials = [('Model A', model)] + dials
    done = set()
    for k, v in dials:
        if k in done:
            continue
        if k.endswith(' Color'):
            base = k[:-6]
            an = V.ALPHA_NAMES.get(base, base + ' Alpha')
            h, s, b, a = d[k], d.get(base + ' Sat', 0), d.get(base + ' Bri', 1), d.get(an, 1)
            r, g, bb = colorsys.hsv_to_rgb(h % 1.0, s, b)
            put(sp, k, '#%02x%02x%02x%02x' % (round(r * 255), round(g * 255), round(bb * 255), round(a * 255)))
            done |= {k, base + ' Sat', base + ' Bri', an}
            continue
        last = [v2 for k2, v2 in dials if k2 == k][-1]  # later dials win
        put(sp, 'Model File' if maskehr and k == 'Model A' else k, last)
        done.add(k)


main()
