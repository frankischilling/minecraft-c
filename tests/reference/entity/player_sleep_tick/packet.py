"""Portable sealed original GS.w_ prefixes; no whole-parent/native qualification."""
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
FINITE='10bf67c2535f015762ae5978bf5d190eccbdeae325d79a8d91c4ba51e71b506c'
ANCHORS={'executed_producer_sha256': '54e1fafd239f9b440f2836822d3138e274278b194f66de47809e3aa506bfa261', 'executed_helper_sha256': '75ee28859c8b86ed66cb39c91f0a4b0e1f849929a8a4bcd4535cb1a0c709bd21', 'execution_binary_input_sha256': 'faf98e82592cbf6db8d3dfeb46e4b7d61cffb3209e4085fc69f536a5a6a7f341', 'data_sha256': '51204c213bf0ad35f71439051af50cd4d71aae86c4294a71be3b34206aafa63b', 'live_code_roles': '93e39b6f40f06bc9bd3b5b09a8ec9c9ebe372a439ed3be4d93211e57d3138d83', 'vm_projections': 'aae5f51bffc5518bf6b2797075044cda0b16814506d54c968fcfc81476a0361f', 'failed_attempts': '476905e00dbb033bb7dd5139fd2f64365477004c2ac798d20763267ccc0be01a', 'accepted_preflight': '13d38ed5f1b49f89269624ccde3941c784f0580fe900b4546934f39c77f75bfa'}
PROOF_KEYS=('schema', 'status', 'declared_repository_head', 'archive_sha256', 'server_archive_sha256', 'profiles_per_vm', 'fresh_matrix_target_vms', 'prefix111_profiles', 'first_source_exceptions', 'selected_calls_per_vm', 'selected_events_per_vm', 'live_code_methods', 'executed_original_method_count', 'source_parent_returns', 'source_terminal_resumes', 'target_method_invocations', 'target_source_writes', 'target_forced_returns', 'planned_target_termination', 'null_final_T_excluded', 'native_qualification', 'whole_parent_qualification', 'gui_qualification', 'constructor_entropy_scope', 'finite_sha256', 'capture_receipt_sha256', 'executed_producer_sha256', 'executed_helper_sha256', 'execution_binary_input_sha256', 'legacy_input_count', 'legacy_before_after_map_sha256', 'vm_projections', 'live_code_roles', 'failed_attempts', 'accepted_preflight', 'data_sha256', 'private_capture_cohort_sha256', 'private_capture_artifact_count', 'historical_private_provenance_sha256', 'source_inputs_unchanged', 'generated_helpers_unchanged', 'original_projections_identical', 'current_reproduction_sha256', 'canonical_exporter_sha256', 'scope')
TOOLS=('PlayerSleepParentObservation.java','PlayerSleepParentJdi.java','capture_reference.py',
       'export_reference.py','validate.py','packet.py','test_packet.py','README.md','.gitattributes',
       'preflight-profiles.json','preflight-profiles.tsv')

def require(value,message):
    if not value:raise ValueError(message)

def digest(path):
    path=Path(path);require(path.stat().st_size<=8*1024*1024,'bounded hashed member')
    return hashlib.sha256(path.read_bytes()).hexdigest()
def map_digest(value):return hashlib.sha256(json.dumps(value,sort_keys=True,separators=(',',':')).encode()).hexdigest()
def pairs(items):
    value={}
    for key,item in items:require(key not in value,'duplicate JSON key');value[key]=item
    return value
def loads(text):
    def invalid(token):raise ValueError('nonfinite JSON token '+token)
    return json.loads(text,object_pairs_hook=pairs,parse_constant=invalid)
def load(path):
    path=Path(path);require(path.stat().st_size<=8*1024*1024,'bounded packet member')
    return loads(path.read_text(encoding='utf-8'))
def exact(a,b):return type(a) is type(b) and (a.keys()==b.keys() and all(exact(a[k],b[k]) for k in a) if type(a) is dict else len(a)==len(b) and all(exact(x,y) for x,y in zip(a,b)) if type(a) is list else a==b)
def digest_map(value,names=None):
    require(type(value) is dict and (names is None or set(value)==set(names)),'closed hash inventory')
    require(all(type(n) is str and n and ':' not in n and chr(92) not in n and not n.startswith('/') and all(p not in ('','.','..') for p in n.split('/')) and type(v) is str and re.fullmatch('[0-9a-f]{64}',v) for n,v in value.items()),'portable logical paths and SHA256')
def validator(directory):
    spec=importlib.util.spec_from_file_location('sleep_parent_portable_validate',Path(directory)/'validate.py')
    value=importlib.util.module_from_spec(spec);spec.loader.exec_module(value);return value

def read_reference(directory=HERE):
    directory=Path(directory);p=load(directory/'provenance.json')
    require(type(p) is dict and set(p)==set(PROOF_KEYS),'closed portable proof')
    require(p['schema']=='PLAYER_SLEEP_TICK_PREFIX_CANONICAL_V1' and p['status']=='PASS_ORIGINAL_PREFIX_COMPONENTS_ONLY' and p['declared_repository_head']=='21effab7','historical source-only declaration')
    counts={'profiles_per_vm':36,'fresh_matrix_target_vms':2,'prefix111_profiles':32,'first_source_exceptions':4,'selected_calls_per_vm':1085,'selected_events_per_vm':3359,'live_code_methods':39,'executed_original_method_count':36,'source_parent_returns':0,'source_terminal_resumes':0,'target_method_invocations':0,'target_source_writes':0,'target_forced_returns':0,'legacy_input_count':259,'private_capture_artifact_count':39}
    for name,value in counts.items():require(type(p[name]) is int and p[name]==value,'typed original count '+name)
    for name,pin in ANCHORS.items():require(map_digest(p[name])==pin,'complete historical map/list '+name)
    for name in ('executed_producer_sha256','executed_helper_sha256','execution_binary_input_sha256','data_sha256'):digest_map(p[name])
    require(p['archive_sha256']=='af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c' and p['server_archive_sha256']=='033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d','immutable archive pins')
    require(p['capture_receipt_sha256']=='37bcace8ee549a7cb0f5378bb44240c90eac4ac77ac3ec9c72f3a8b6a747b816' and p['historical_private_provenance_sha256']=='5a51d995c6280b90cbd4efcd2332cd551d3513839a170dd0b65aa3b4aaec280e' and p['legacy_before_after_map_sha256']=='893ccc0351ba71ef602328254e1ca29c950dd7b65a108f4df07ca3b6aae2403e' and p['private_capture_cohort_sha256']=='9373f5d9fbfd206f54657bbf1c50c4bd4dcd7f701490eed03102e03a5597449e','historical sealed receipt identities')
    require(p['finite_sha256']==FINITE and p['planned_target_termination']=='JDWP_VM_EXIT_0' and p['null_final_T_excluded'] is True,'actual unresumed termination scope')
    require(all(p[n] is False for n in ('native_qualification','whole_parent_qualification','gui_qualification')) and all(p[n] is True for n in ('source_inputs_unchanged','generated_helpers_unchanged','original_projections_identical')),'bounded historical qualification')
    require(p['constructor_entropy_scope']=='within-call preservation only; no cross-VM constructor equality','within-call entropy scope')
    require(p['scope']=='Original GS.w_ prefixes only:32 actual unresumed111 frames and4 first NPE frames. Native whole-tick continuation is a separate declared input/coverage scope.','bounded canonical scope')
    require(len(p['execution_binary_input_sha256'])==152 and len(p['executed_helper_sha256'])==4,'complete historical tool/helper inventory')
    digest_map(p['current_reproduction_sha256'],TOOLS)
    for group in ('data_sha256','current_reproduction_sha256'):
        for name,pin in p[group].items():
            require(Path(name).name==name and digest(directory/name)==pin,'canonical/current member '+name)
    require(p['canonical_exporter_sha256']=={n:p['current_reproduction_sha256'][n] for n in ('export_reference.py','validate.py','packet.py')},'current exporter identities remain separate')
    for name in ('PlayerSleepParentObservation.java','PlayerSleepParentJdi.java'):
        require(digest(directory/name)==p['executed_producer_sha256']['producer/'+name],'unchanged actual executed Java producer')
    require(digest(directory/'finite.json')==FINITE,'immutable original finite observations')
    return validator(directory).validate_canonical(load(directory/'finite.json'),directory)

def profiles(directory=HERE):
    read_reference(directory)
    return load(Path(directory)/'profiles.json')
if __name__=='__main__':
    print('Original sleep-parent prefixes:',len(read_reference()['rows']),'rows; whole parent return unqualified')
