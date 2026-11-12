"""Portable original GS sleep/wake components; no native or tick qualification."""
import hashlib
import json
from pathlib import Path
import re
import sys
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE))
from validate import (JAR,digest,exact,integer,keys,load,profiles,require,validate_canonical)

DATA=('finite.json','profiles.json','profiles.tsv','source-pins.json','helper-pins.json','class-names.txt')
SOURCES=('.gitattributes','BedNightObservation.java','BedNightJdi.java','capture_reference.py',
         'export_reference.py','validate.py','packet.py','test_packet.py','README.md')
FINITE='11e7210eda8b1a5d4b79e383bbbc385c55f192377a02d31ee9b350d8de89c6cc'
# Filled from the sealed historical evidence, never from revised reproduction tools.
HISTORICAL={'canonical_sha256': 'f8d317e2a9bf5c42f1d7202405f9c450ae937a0fd535c310d281cf8c9a6b15de', 'executed_producer_sha256': '2685d7f7a19b80b75e6190cc58e5e382aad60c3cae3c0905f3f56f9f86aa142e', 'helper_sha256': '3109f866032ed9f279aad1da1792ea8600e6fdaa6531246596c19386f9e25ce0', 'binary_input_sha256': 'f46de75412b8f428ef61c845c33a00f1b9e47ef87690509cccc0bd9b09a132ce', 'version_receipt_sha256': '1f970f8ea7e42c509e5046e76a7385e1ada62b6df600b2db43fd3b193e1f1f41', 'private_evidence_sha256': '4aaae12d223d1e0bba29fd4347f82e50fb0a68c54d24b0e84535976898d1dc50', 'historical_audit_sha256': '20efbdeef06a024427d6dcb6154a1819c88b2660d38aebcf896a395bb4dc5397'}

def map_digest(value):
    return hashlib.sha256(json.dumps(value,sort_keys=True,separators=(',',':')).encode('ascii')).hexdigest()

def digest_map(value,names=None):
    require(type(value) is dict and (names is None or set(value)==set(names)),'closed historical pin inventory')
    require(all(type(n) is str and n and ':' not in n and chr(92) not in n and not n.startswith('/') and all(p not in ('','.','..') for p in n.split('/')) and type(v) is str and re.fullmatch('[0-9a-f]{64}',v) for n,v in value.items()),'portable member roles and SHA256 values')

def bounded(path,maximum=4*1024*1024):
    path=Path(path);require(path.stat().st_size<=maximum,'bounded canonical member');return path

def read_reference(directory=HERE):
    """Read finite selected values/journals without a JAR, JVM or raw attempt tree."""
    directory=Path(directory);p=load(bounded(directory/'provenance.json'))
    counts={'profiles_per_vm':76,'fresh_target_vms':2,'sleep_successes':16,'wake_successes':56,
            'source_null_prefixes':4,'live_code_methods':26,'original_classes':16,
            'selected_calls_per_vm':1313,'ordered_events_per_vm':3979,
            'historical_rejected_corruptions_per_audit':687,'jdi_target_invocations':0,
            'jdi_source_writes':0,'jdi_forced_returns':0,'jdi_redefinitions':0}
    keys(p,('schema','status','archive_sha256','source_inputs_unchanged','generated_helpers_unchanged',
            'original_projections_identical','source_guard_counters','canonical_sha256',
            'executed_producer_sha256','current_reproduction_sha256','canonical_exporter_sha256',
            'helper_sha256','binary_input_sha256','version_receipt_sha256','private_evidence_sha256',
            'historical_audit_sha256','vm_projections','scope','eol',*counts),'closed canonical proof')
    require(p['schema']=='PLAYER_SLEEP_WAKE_CANONICAL_V1' and p['status']=='PASS_ORIGINAL_COMPONENTS_ONLY' and p['archive_sha256']==JAR and p['source_inputs_unchanged'] is True and p['generated_helpers_unchanged'] is True and p['original_projections_identical'] is True,'original-only frozen qualification')
    for name,value in counts.items():require(type(p[name]) is int and p[name]==value,'typed original count '+name)
    require(exact(p['source_guard_counters'],{'network':0,'native_links':0,'writes_deletes':0,'exec':0}),'actual guarded target counters')
    for name,anchor in HISTORICAL.items():
        digest_map(p[name]);require(map_digest(p[name])==anchor,'immutable complete historical map '+name)
    for name,pin in p['helper_sha256'].items():
        helper=load(directory/'helper-pins.json')['classes'][name[:-6]]
        require(helper['class_sha256']==pin,'executed authored helper class pin')
    for name in ('BedNightObservation.java','BedNightJdi.java'):
        require(digest(directory/name)==p['executed_producer_sha256'][name],'unchanged executed Java producer bytes')
    digest_map(p['canonical_sha256'],DATA);digest_map(p['current_reproduction_sha256'],SOURCES)
    for group in ('canonical_sha256','current_reproduction_sha256'):
        for name,pin in p[group].items():
            member=bounded(directory/name);require(digest(member)==pin,'canonical/current tool continuity '+name)
            require(b'\r' not in member.read_bytes(),'explicit LF member bytes '+name)
    require(digest(directory/'finite.json')==FINITE,'immutable canonical finite observations')
    require(p['canonical_exporter_sha256']=={n:p['current_reproduction_sha256'][n] for n in ('export_reference.py','validate.py','packet.py')},'current exporter separated from actual executed producers')
    require(p['eol']=='LF for all package members; executed Java bytes already LF; no captured member normalized.','explicit original/canonical byte policy')
    require(type(p['scope']) is str and p['scope'].startswith('Original components only;'),'bounded source scope')
    projection={'finite_sha256':FINITE,'profiles':76,'class_bindings':16,'calls':1313,'events':3979,'live_code':26,'target_exit':0,'timed_out':False,'capture_complete':True,'overflow':False,'reader_errors':0}
    require(type(p['vm_projections']) is list and len(p['vm_projections'])==2 and all(exact(v,projection) for v in p['vm_projections']),'two independently checked original projections')
    return validate_canonical(load(bounded(directory/'finite.json')),directory)

if __name__=='__main__':
    print('Original Player sleep/wake components:',len(read_reference()['rows']),'rows; no tick/GUI/native qualification')
