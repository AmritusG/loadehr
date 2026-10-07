#!/usr/bin/env python3
"""W21: set LoadEhr clip source params by NAME over Arena's REST API (never triggers a clip).
usage: w21_arena.py <clip id> get [name...] | set name=value [name=value ...]  (choices by option name, \\n = row break)"""
import json, sys, urllib.request
API = 'http://127.0.0.1:8080/api/v1'
def req(method, path, body=None):
    r = urllib.request.Request(API + path, method=method, data=None if body is None else json.dumps(body).encode(),
                               headers={'Content-Type': 'application/json'})
    with urllib.request.urlopen(r, timeout=10) as f:
        t = f.read()
        return json.loads(t) if t else None
def params(cid):
    return req('GET', '/composition/clips/by-id/%s' % cid)['video']['sourceparams']
def main():
    cid, cmd, rest = sys.argv[1], sys.argv[2], sys.argv[3:]
    sp = params(cid)
    if cmd == 'get':
        for n in rest or sp:
            print('%s = %r' % (n, sp.get(n, {}).get('value', '<missing>')))
        return
    for kv in rest:
        n, v = kv.split('=', 1)
        if n not in sp:
            print('MISSING', n); continue
        p = sp[n]; t = p['valuetype']
        if t == 'ParamChoice':
            val = v if v in p['options'] else p['options'][int(v)]
        elif t == 'ParamBoolean':
            val = v.lower() in ('1', 'true', 'on')
        elif t in ('ParamString', 'ParamFile', 'ParamText', 'ParamColor'):
            val = v.replace('\\n', '\n')
        else:
            val = float(v)
        req('PUT', '/parameter/by-id/%d' % p['id'], {'value': val})
        print('set %s = %r' % (n, val))
main()
