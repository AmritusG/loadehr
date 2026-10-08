#!/usr/bin/env python3
"""LoadEhr W3: Arena cost of the W3 DXV clips (layer 5 = "LoadEhr W3"). Per column: Debug Log on, the clip SELECTED (never
triggered: the selected clip renders for the clip-panel preview, MASKEHR-002), 20 s of PERF + VIDEO log lines, Debug Log off.
Park the selection on an empty slot afterwards. usage: tools/w3_arena_perf.py <col> ..."""
import json, urllib.request, time, os, re, sys
API='http://127.0.0.1:8080/api/v1'; LOG=os.path.expanduser('~/Library/Logs/LoadEhr/LoadEhr.log')
def req(m,p,b=None):
    r=urllib.request.Request(API+p,method=m,data=None if b is None else json.dumps(b).encode(),headers={'Content-Type':'application/json'})
    with urllib.request.urlopen(r,timeout=10) as f:
        t=f.read(); return json.loads(t) if t else None
def setp(col,name,val):
    c=req('GET','/composition/layers/5/clips/%d'%col); p=c['video']['sourceparams'][name]
    req('PUT','/parameter/by-id/%d'%p['id'],{'value':val}); return c['id']
for col in map(int,sys.argv[1:]):
    cid=setp(col,'Debug Log',True)
    req('POST','/composition/clips/by-id/%d/select'%cid)
    time.sleep(6); mark=os.path.getsize(LOG); time.sleep(20)
    log=open(LOG,errors='replace').read()[mark:]
    perf=[l for l in log.splitlines() if ' PERF ' in l]; vid=[l for l in log.splitlines() if ' VIDEO ' in l]
    print('C%d'%col); [print('  ',l[24:250]) for l in perf[-3:]+vid[-3:]]
    setp(col,'Debug Log',False)
