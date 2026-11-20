"""Integrity and semantic qualification of the finite original Torch particles."""
import hashlib
import importlib.util
import json
from pathlib import Path
import sys

sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
DIGESTS={
 'audit.py':'b58c418cebbce59bb3d70bde46e4e5ecc642a244b21e21a2897496b22ae89d76',
 'capture_reference.py':'fdc5756a39c067e138d974b76ece1b2db04f6badbe5021691efe2c29fad2575d',
 'compare.py':'f682e4dd6e9eb488edc448659d824052094a26116e21dcd8f80a582235533605',
 'constructor-evidence.json':'c392bfd493f0696fd6cd5ce961ad4adecf6512f92572b15329c8d42ba140c047',
 'declared-inputs.json':'a0e1a28ddcc7b9416dd13ac80f381beb836eb0fcfd6f900c9797cde2b288c6a0',
 'finite.json':'de55426fe777afd016f17273b0ca1ab9e58c96b6b9dc5b6dd1d4cafc97ac8dec',
 'observed-trace.json':'6b51f8bc5bb202d53d2b38ad7fc88cbed85d9d75127173a29b0f06f21fb82fff',
 'profiles.tsv':'918afff21b350729175e794b1475f2e1cf5f85f1ca9cbb175045c7d5ddf621ca',
 'provenance.json':'dadd2d5c5b3d3f54f7f1e0957f80b0888f8f1032b3e68211ec89d27d4805e61e',
 'source-code.json':'5359d7ebaab42f9de3888916d693ccd36eedcccf1b89db228c64631e15e0068e',
 'TorchParticleJdi.java':'4c72cb920b1ae77bf5ad4b8d1156ce7d94279cea5fef528659e410aaf430f4e5',
 'TorchParticleObservation.java':'1b44dedf9f01f8e484c510da1627d2c7ba1462981cffc4c670efa6b8803dcb04'}


def require(value,message):
    if not value:raise ValueError(message)


def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path);result=importlib.util.module_from_spec(spec);spec.loader.exec_module(result);return result


A=module('torch_packet_entropy',HERE/'audit.py')
P=module('torch_packet_comparison',HERE/'compare.py')


def load(name):return json.loads((HERE/name).read_text())


def receipt(row):
    require(type(row['exit']) is int and row['exit']==0 and row['timed_out'] is False and
            row['pipe_capture_complete'] is True and row['reader_errors']=={} and row['output_overflow'] is False,
            'complete successful actual process receipt')


def semantic_hash(value):
    return hashlib.sha256(json.dumps(value,sort_keys=True,separators=(',',':')).encode()).hexdigest()


def qualify(root=HERE):
    for name,expected in DIGESTS.items():
        require(hashlib.sha256((root/name).read_bytes()).hexdigest()==expected,'packet fingerprint '+name)
    evidence=json.loads((root/'constructor-evidence.json').read_text());finite=json.loads((root/'finite.json').read_text())
    provenance=json.loads((root/'provenance.json').read_text());pins=json.loads((root/'source-code.json').read_text())
    trace=json.loads((root/'observed-trace.json').read_text())
    require(provenance['status']=='PASS' and provenance['vms']==3 and provenance['all_source_and_product_hashes_before_after_equal'] is True,'three stable guarded original VMs')
    for name,expected in provenance['producer_hashes'].items():require(DIGESTS[name]==expected,'frozen captured producer '+name)
    for row in provenance['ordinary_receipts']+[provenance['debugger_controller_receipt'],provenance['compile_receipt']]:receipt(row)
    require(len(evidence['ordinary'])==2 and len(provenance['ordinary_receipts'])==2,'two fresh ordinary VMs')
    for rows,expected in zip(evidence['ordinary'],provenance['ordinary_entropy']):
        require(A.ordinary(rows)==expected,'independent ordinary constructor arithmetic and entropy audit')
    require(A.debugger(evidence['debugger'],evidence['debugger_returned'],pins)==provenance['debugger_entropy'],'direct JDI entropy/Code and same-process returned state')
    require(P.trace(trace['events'],trace['live_code'],pins,finite['components'])==provenance['debugger_trace'],'actual full component source traces and retained platform exceptions')
    require(provenance['component_semantic_sha256_per_vm']==[semantic_hash(finite['components'])]*3,'all three returned deterministic projections agree exactly')
    require(len(finite['components'])==30 and sum(len(r['packed'][:r['nw']['words']]) for r in finite['components'] if r['nw'])==384,'all384 actual active sprite words')
    require(provenance['own_prior_unchanged_words_per_vm']==25165440,'full actual inactive-word audit extent')
    require(provenance['client_jar_sha256']==pins['client_jar_sha256']=='af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c','original archive pin')
    return finite['components']


if __name__=='__main__':qualify();print('Torch particle packet:30 components,37 constructors in each of3 actual VMs')
