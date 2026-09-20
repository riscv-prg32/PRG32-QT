#!/usr/bin/env python3
"""Enumerate a PRG32 Store, download every portable PRG2 package, and run it headlessly."""
import argparse,json,os,subprocess,sys,tempfile,urllib.request,urllib.parse
DEFAULT="http://193.205.230.7:5080"
def get(url):
    req=urllib.request.Request(url,headers={'User-Agent':'PRG32-QT-store-certification/0.3'})
    with urllib.request.urlopen(req,timeout=20) as r:return r.read()
def join(base,path):return urllib.parse.urljoin(base.rstrip('/')+'/',path)
def items_from(data):
    if isinstance(data,list):return data
    if isinstance(data,dict):
        for k in ('games','cartridges','items','results'):
            if isinstance(data.get(k),list):return data[k]
    return None
def catalog(store):
    # Match the iOS client: try the modern game API first, then discovery/catalog fallbacks.
    try:
        x=items_from(json.loads(get(join(store,'api/games'))))
        if x is not None:return x
    except Exception:pass
    disc=json.loads(get(join(store,'.well-known/prg32-store.json')))
    if disc.get('abi')!='prg32-store-discovery-1.0':raise RuntimeError('unsupported store discovery ABI')
    eps=[]
    for k in ('catalog','catalog_url','cartridges','cartridges_url','api'):
        if isinstance(disc.get(k),str):eps.append(disc[k])
    eps += ['api/games','api/cartridges','cartridges.json','catalog.json']
    for ep in eps:
        try:
            x=items_from(json.loads(get(join(store,ep))))
            if x is not None:return x
        except Exception:pass
    raise RuntimeError('no supported catalog endpoint')
def download_url(store,item):
    direct=item.get('download_url') or item.get('url') or item.get('package_url') or item.get('cartridge_url')
    variants=item.get('variants')
    if isinstance(variants,dict):direct=variants.get('qt') or variants.get('qemu') or variants.get('esp32c6') or variants.get('ios') or direct
    if direct:return join(store,direct)
    ident=str(item.get('id') or '')
    if not ident:return None
    offered=[str(x) for x in item.get('architectures',[]) if isinstance(x,str)]
    arch=next((x for x in ('qt','qemu','esp32c6','ios') if x in offered),'qemu')
    base=join(store,'api/games/'+urllib.parse.quote(ident,safe='')+'/download')
    q={'architecture':arch}
    if item.get('version'):q['version']=str(item['version'])
    return base+'?'+urllib.parse.urlencode(q)
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--store',default=DEFAULT);ap.add_argument('--runner',default='./build-core/prg32qt-headless');ap.add_argument('--frames',type=int,default=300);a=ap.parse_args()
    items=catalog(a.store);failures=[]
    with tempfile.TemporaryDirectory() as td:
        for n,item in enumerate(items):
            if not isinstance(item,dict):failures.append((f'item-{n}','invalid catalog object'));continue
            ident=str(item.get('id') or item.get('name') or f'cart-{n}')
            u=download_url(a.store,item)
            if not u:failures.append((ident,'no download URL or id'));continue
            try:data=get(u)
            except Exception as ex:failures.append((ident,f'download: {ex}'));continue
            if not data.startswith(b'PRG2'):failures.append((ident,'not PRG2'));continue
            path=os.path.join(td,ident.replace('/','_')+'.prg32');open(path,'wb').write(data)
            p=subprocess.run([a.runner,path,str(a.frames)],capture_output=True,text=True)
            if p.returncode:failures.append((ident,(p.stderr or p.stdout).strip()))
            else:print(p.stdout.strip())
    print(f'catalog: {len(items)} cartridge(s); failures: {len(failures)}')
    for x in failures:print('FAIL',*x,sep=': ',file=sys.stderr)
    return 1 if failures else 0
if __name__=='__main__':raise SystemExit(main())
