"""Audit the finite unchanged-original Dye icon packet."""
import hashlib
import json
from pathlib import Path
PINS = {'capture-provenance.json': '0633cefc3796143c317b1a335f52d9c32c1b2a2daf4e435764d576eece49b3de', 'declared-inputs.json': 'c4f436fa28fb19b4e2f70cda5cd499b84e43690ac807fabf10cc5396ed822e63', 'DyeIconObservation.java': '913ed750634cdf928b42f749217d2fceebbcf076c8c4c1d7eb6ad03abdf932d3', 'failed-initialization.stderr-projection.txt': '586a48cc0b3bc68af2ea567b071cf32b25e6886d6a5ba8aedcbfba731f1883be', 'failed-initialization.stdout': 'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855', 'ordinary-0.stdout': '10bd7618540fe675a2a1104736e928de01fc6dbc974957a9194e8369cf86ea30', 'ordinary-1.stdout': '10bd7618540fe675a2a1104736e928de01fc6dbc974957a9194e8369cf86ea30', 'profiles.tsv': '4bb99dc6dee70fd299999e5bd06f4485d360e042c6aa93cf3b32f165a4e646c3', 'README.md': 'e8c39f81b85cbc8836218c6dbd3a6e4ac0fd57da1cedf82e0214b2fabb76baa7', 'selected.jsonl': 'c773ca56a3a31c549da5ea2b02ff2ff64b5682e16a43ed167939ba92a5392353', 'source-pins.json': '2029982a61b84c4bff714b3043cb56ba4e63697119171b202b5745830f951fa9'}
SELECTED = 'c773ca56a3a31c549da5ea2b02ff2ff64b5682e16a43ed167939ba92a5392353'
KEYS = {'index','name','bh','damage','direct_icon','item_stack_icon','stack_icon','color','full3d','rotate'}
def require(ok, text):
    if not ok: raise ValueError(text)
def read_packet(directory=None):
    base=Path(directory) if directory is not None else Path(__file__).resolve().parent
    for name,want in PINS.items():
        require(hashlib.sha256((base/name).read_bytes()).hexdigest()==want,'artifact pin: '+name)
    raw=[(base/('ordinary-'+str(i)+'.stdout')).read_bytes() for i in range(2)]
    require(raw[0]==raw[1],'fresh VM repeat')
    selected=(base/'selected.jsonl').read_bytes()
    require(hashlib.sha256(selected).hexdigest()==SELECTED,'selected pin')
    require(selected==('\n'.join([s for s in raw[0].decode('ascii').splitlines() if s.startswith('DYE_ICON')])+'\n').encode('ascii'),'declared LF projection')
    lines=selected.decode('ascii').splitlines()
    require(lines[0]=='DYE_ICON_INIT\t{"id":351,"initial_bh":78}','actual registered initial icon')
    rows=[json.loads(s.split('\t',1)[1]) for s in lines[1:-1]]
    profiles=[s.split('\t') for s in (base/'profiles.tsv').read_text().splitlines()]
    require(len(rows)==len(profiles)==33,'complete finite inventory')
    for i,(row,p) in enumerate(zip(rows,profiles)):
        require(set(row)==KEYS,'literal row schema')
        require(row['index']==i and row['name']==p[0] and [row['bh'],row['damage']]==list(map(int,p[1:])),'declared ordered operands')
        require(all(type(row[k]) is int for k in KEYS-{'name','full3d','rotate'}),'signed scalar types')
        require(type(row['full3d']) is bool and type(row['rotate']) is bool,'Boolean leaf types')
        require(row['direct_icon']==row['item_stack_icon']==row['stack_icon'],'observed wrapper equality')
    require(lines[-1]=='DYE_ICON_COMPLETE rows=33 network=0 native=0 execs=0 writes=0 restored_bh=78','completion')
    receipt=json.loads((base/'capture-provenance.json').read_text())
    require(receipt['status']=='PASS' and receipt['ordinary_fresh_vms']==2 and receipt['original_game_class_replacements']==0,'repeat receipt')
    require(receipt['inputs_before']==receipt['inputs_after'] and receipt['inputs_unchanged'] and receipt['helpers_unchanged'],'frozen continuity')
    return rows
if __name__=='__main__':
    print('Dye icon original packet PASS: '+str(len(read_packet()))+' rows')
