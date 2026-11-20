"""Closed Java-free reader for bounded unchanged-original Redstone Ore calls."""
import importlib.util
from pathlib import Path
import sys
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]


def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    value=importlib.util.module_from_spec(spec);spec.loader.exec_module(value)
    return value


F=module('ore_sealed_fire_packet',HERE.parent/'fire/packet.py')
P,G=F.P,F.G
D=module('ore_declared_profiles',HERE/'profiles.py')
M=module('ore_independent_model',HERE/'model.py')
model,plane_arrays,planes,RandomModel=M.model,M.plane_arrays,M.planes,M.RandomModel
SCHEMA='REDSTONE_ORE_V1'
PROVENANCE='REDSTONE_ORE_PROVENANCE_V1'
SCOPE='Sixty-seven declared unchanged-original Redstone Ore calls with genuine prepared World, Chunks and selected controller callers; natural encounters, rendering, frame rate and whole-game parity remain separate.'
SOURCES=('RedstoneOreObservation.java','profiles.py','profiles.json','model.py','packet.py',
         'capture_reference.py','test_packet.py','README.md','ore-method-pins.json')
SHARED={**F.SHARED,**{'fire_'+name:F.HERE/name for name in (*F.SOURCES,'finite.json','provenance.json')}}
HELPERS=F.HELPERS|{'RedstoneOreObservation.class','RedstoneOreObservation$Profile.class','RedstoneOreObservation$Observer.class'}
HELPER_IDENTITIES={'ConstructorJson.class': '1c13bbd4aa1dc7928a37d158835f81cb977a33d3699d34d45a73e9cbacdd462a', 'FireObservation$Observer.class': '97076cf6c9294fae982cc4367635ac5afe31ea59bdb725714010378d1f150a76', 'FireObservation$Profile.class': '147df6176b761ee0ea96718e100cb5fabb96ca1f7ff27234b0fb5658963dc0fc', 'FireObservation.class': 'cbb7b4a39a6ee5f200504142b91c65eb2e0ec04b95998ef0c6c2791b5e50a900', 'PlayerDamageWakeObservation$Guard.class': 'd8f4514d89f717720edc16f79176ed57bc486e36ecf253f3920f15a2c13ee84a', 'PlayerDamageWakeObservation$Observer.class': 'cdc6d8e926a1d52d305c08b75eb599fef548d36ca17cbaf5cef8ee575de76efc', 'PlayerDamageWakeObservation.class': 'afe16cd0c098beb59c453265d36082c66faea673673f481f0a375841ad000ad0', 'RedstoneOreObservation$Observer.class': 'ca4765a7c12afa2f35d18d46a58e84ce294704be8c2e83d9ddbd7e1a56676ec6', 'RedstoneOreObservation$Profile.class': 'aacbd1e484cf2be123b364086b4151b2551ba6cf4de914e53e4277af156ac810', 'RedstoneOreObservation.class': '10bbd53711c7cbf3185e7392914c89998069c1945b0f7962f65003f6c523bd57', 'UprightPlantsWakeObservation$Observer.class': '4b12128a03e91ab030f8bf91c7af852d2c2f8a5be591b51c123649ef056649ce', 'UprightPlantsWakeObservation$Profile.class': '9c889ad9a7219a80cd12df0ccbacf45eb519131e99a1f3cc52747499964ca317', 'UprightPlantsWakeObservation.class': '69b3b13853db4d808e850e47a75288d3ead38ba2842cdbf0bd27f4aeddcfb76e'}
BINARIES,RECEIPT_KEYS,PROCESS_TIMEOUTS=F.BINARIES,F.RECEIPT_KEYS,F.PROCESS_TIMEOUTS
public_receipt,class_record,receipt=F.public_receipt,F.class_record,F.receipt
CLASS_IDENTITIES={'bs': (1781, '477924f2e48c7eefff8e1df450c23253ca353cc723405a5ac1d1a949e3c81675', 'uu', 'f3c4645fb4c3f732b3da71d1f780514e44d35fd421e82050adc6b18ec9c044ea'), 'os': (2383, 'd53eb2ca4c005c2dbc2fbdb752611f33615ebf403d031109b901482814238b1f', 'ob', '63ee34d7e89ef4c5d368cc5c2785e51a898a5258d559be1c2171101037c99bc0'), 'ob': (2260, 'c367f0a0b4467234a3d30690106b2adde8ad74f7c80db285fefe1f75101c3147', 'java/lang/Object', 'af19091f72cdf9f8625d9ef0161510d419194c167c8c0d47708fcc0982a05abb'), 'ck': (1239, 'fe3d757cd56ac42ef8e0c0aaecf5fc7878406fe66db88ecf8061456f50cdd2eb', 'gm', '11348f1b2897d6b235d8164764eb13418b569e41ae27a73dea3f1686a457b00f'), 'ct': (698, 'eec90ef477d6ac92027e6ceade79fcf33e09ee65697f29dccd018503cc0de982', 'java/lang/Object', '1b2de850915b444ca60d68282217fc176823607b5cf5f75d6c681aff9fcde697')}
MODES=tuple(p['mode'] for p in D.declared())


def profiles():
    result=P.load(HERE/'profiles.json')
    P.require(P.exact(result,D.declared()),'fixed ordered Redstone Ore operands')
    return result


def sources():return {name:P.digest(HERE/name) for name in SOURCES}
def shared():return {name:P.digest(path) for name,path in SHARED.items()}


def class_pins():
    extra=P.load(HERE/'ore-method-pins.json')
    P.require(type(extra) is dict and set(extra)=={'archive_sha256','classes'} and extra['archive_sha256']==P.JAR and
              type(extra['classes']) is dict and set(extra['classes'])==set(CLASS_IDENTITIES),'closed Ore class inventory')
    for name,(size,digest,parent,structure) in CLASS_IDENTITIES.items():
        class_record(extra['classes'][name],name,size,digest,parent)
        P.require(G.structural_hash(extra['classes'][name])==structure,'complete unchanged original Ore class and Code')
    base=F.class_pins()
    return dict(archive=base['archive'],classes={**base['classes'],**extra['classes']})


def method_pins():
    result=F.method_pins()
    for name in CLASS_IDENTITIES:
        for method in class_pins()['classes'][name]['methods']:
            if method['code_length'] is not None:
                result[name+'.'+method['name']+method['descriptor']]={k:method[k] for k in ('code_length','code_sha256','code_attribute_sha256')}
    return result


def full_trace(value,source,kind,exception=False):
    F.F.trace(value)
    callback='b' if kind=='range' else 'e' if kind=='sky' else 'a'
    prefix=[] if exception else ['java.lang.Thread.getStackTrace','RedstoneOreObservation$Observer.event']
    if exception and kind is not None:prefix.append('RedstoneOreObservation$Observer.interrupt')
    if kind is not None:prefix.append('RedstoneOreObservation$Observer.'+callback)
    expected=[*prefix,*source,'RedstoneOreObservation.run','RedstoneOreObservation.main']
    P.require(P.exact([f['class']+'.'+f['method'] for f in value],expected),'complete genuine Ore callback/Throwable frame order')
    for frame in value:
        file='Thread.java' if frame['class']=='java.lang.Thread' else 'RedstoneOreObservation.java' if frame['class'].startswith('RedstoneOreObservation') else 'SourceFile'
        P.require(frame['file']==file,'unchanged original or authored frame file')


def validate(rows):
    P.require(type(rows) is list and len(rows)==len(MODES),'complete ordered Ore row inventory')
    prepared=dict(scheduled=dict(initialized=False,sequence='0'*16),entity=dict(initialized=False,next_id=0),item_use_stat=0)
    for row,profile in zip(rows,profiles()):
        P.require(type(row) is dict and set(row)=={'mode','prepared','prepared_class','receiver','before','after','result','exception',
                  'exception_message','exception_stack','events'} and row['mode']==profile['mode'],'closed ordered Ore row')
        P.require(P.exact(row['prepared'],profile) and P.exact(row['prepared_class'],prepared),'literal operands and unnormalized prepared source class state')
        expected=model(profile,prepared)
        P.require(P.exact(row['receiver'],dict(**{'class':'bs'},id=profile['entry_id'],lit_flag=profile['entry_id']==74)),'actual concrete Ore receiver and constructor flag')
        for phase in ('before','after'):
            P.require(P.exact(row[phase],dict(expected[phase],chunk_planes=planes(profile,phase=='after',expected))),
                      'independent complete Ore '+phase+' state and all five Chunk planes: '+profile['mode'])
        for key in ('result','exception','exception_message'):
            P.require(P.exact(row[key],expected[key]),'independent Ore outcome '+key+': '+profile['mode'])
        P.require(type(row['events']) is list and len(row['events'])==len(expected['events']),'ordered actual Ore effect count')
        for actual,prediction in zip(row['events'],expected['events']):
            P.require(type(actual) is dict and set(actual)==(set(prediction)-{'source_stack'})|{'stack'} and
                      P.exact({k:v for k,v in actual.items() if k!='stack'},{k:v for k,v in prediction.items() if k!='source_stack'}),
                      'independent ordered Ore parameters, entropy and partial effects: '+profile['mode'])
            full_trace(actual['stack'],prediction['source_stack'],prediction['kind'])
        if expected['exception']=='NONE':P.require(P.exact(row['exception_stack'],[]),'no invented Ore Throwable')
        else:full_trace(row['exception_stack'],expected['exception_source_stack'],expected['events'][-1]['kind'] if expected['exception']=='java/lang/IllegalStateException' else None,True)
        after=expected['after']
        prepared=dict(scheduled={k:after['scheduled'][k] for k in ('initialized','sequence')},entity=dict(after['entity_class']),item_use_stat=after['item_use_stat'])


def validate_finite(value):
    P.require(type(value) is dict and set(value)=={'schema','scope','rows'} and value['schema']==SCHEMA and value['scope']==SCOPE,'closed original Ore envelope')
    validate(value['rows'])


def validate_provenance(proof,directory=HERE):
    directory=Path(directory)
    keys={'schema','status','archive_sha256','finite_sha256','capture_sha256','sources','shared','methods','helpers','binaries','class_count','rows_per_vm',
          'original','processes','inputs_unchanged','helpers_unchanged','class_names_unchanged','class_pins_unchanged'}
    P.require(type(proof) is dict and set(proof)==keys and proof['schema']==PROVENANCE and proof['status']=='PASS_ORIGINAL_CPU_COMPONENTS_ONLY' and
              proof['archive_sha256']==P.JAR and proof['finite_sha256']==P.digest(directory/'finite.json') and P.word(proof['capture_sha256'],64) and
              P.exact(proof['class_count'],len(class_pins()['classes'])) and P.exact(proof['rows_per_vm'],len(MODES)) and
              all(proof[k] is True for k in ('inputs_unchanged','helpers_unchanged','class_names_unchanged','class_pins_unchanged')),'closed Ore provenance and preservation')
    for key,names in (('sources',SOURCES),('shared',SHARED),('helpers',HELPERS),('binaries',BINARIES)):F.F.fingerprints(proof[key],names,'closed '+key+' inventory')
    P.require(P.exact(proof['sources'],sources()) and P.exact(proof['shared'],shared()) and P.exact(proof['methods'],method_pins()) and P.exact(proof['helpers'],HELPER_IDENTITIES) and
              proof['binaries']['client']==P.JAR and all(proof['binaries'][n]==h for n,h in P.LIBRARIES.items()),'actual unchanged Ore Code and source bindings')
    P.require(type(proof['processes']) is dict and set(proof['processes'])==set(PROCESS_TIMEOUTS),'bounded JDK tool inventory')
    for label,timeout in PROCESS_TIMEOUTS.items():receipt(proof['processes'][label],label,timeout)
    P.require(type(proof['original']) is list and len(proof['original'])==2,'two fresh original VMs')
    for i,r in enumerate(proof['original']):receipt(r,'original-'+str(i),70,original=True)
    P.require(all(P.exact(proof['original'][0][k],proof['original'][1][k]) for k in ('exit','stdout_bytes','stderr_bytes','stdout_sha256','stderr_sha256')),'byte-identical complete unfiltered original streams')


def read_reference(directory=HERE):
    directory=Path(directory);value=P.load(directory/'finite.json')
    validate_finite(value);validate_provenance(P.load(directory/'provenance.json'),directory)
    return value


def check(directory=HERE):
    value=read_reference(directory);print('Original Redstone Ore packet:',len(MODES),'fixed inputs and bindings pass')
    return value


if __name__=='__main__':check()
