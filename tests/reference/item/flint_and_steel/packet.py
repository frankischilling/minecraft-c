"""Closed Java-free reader for bounded unchanged-original Flint and Steel calls."""
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


O=module('flint_sealed_ore_packet',HERE.parents[1]/'world/redstone_ore/packet.py')
F=O.F
P,G=F.P,F.G
D=module('flint_declared_profiles',HERE/'profiles.py')
M=module('flint_independent_model',HERE/'model.py')
model,plane_arrays,planes,RandomModel=M.model,M.plane_arrays,M.planes,M.RandomModel
SCHEMA='FLINT_AND_STEEL_V1'
PROVENANCE='FLINT_AND_STEEL_PROVENANCE_V1'
SCOPE='One hundred four declared unchanged-original Flint and Steel YE/IZ/OB calls over genuinely prepared Worlds, Chunks, App, Player and statistics; natural encounters, rendering, frame rate and whole-game parity remain separate.'
SOURCES=('FlintAndSteelObservation.java','profiles.py','profiles.json','model.py','packet.py',
         'capture_reference.py','test_packet.py','README.md','flint-method-pins.json')
SHARED={**O.SHARED,**{'ore_'+name:O.HERE/name for name in (*O.SOURCES,'finite.json','provenance.json')}}
HELPERS=F.HELPERS|{'FlintAndSteelObservation.class','FlintAndSteelObservation$Profile.class','FlintAndSteelObservation$Observer.class'}
HELPER_IDENTITIES={'ConstructorJson.class': '1c13bbd4aa1dc7928a37d158835f81cb977a33d3699d34d45a73e9cbacdd462a', 'FireObservation$Observer.class': '97076cf6c9294fae982cc4367635ac5afe31ea59bdb725714010378d1f150a76', 'FireObservation$Profile.class': '147df6176b761ee0ea96718e100cb5fabb96ca1f7ff27234b0fb5658963dc0fc', 'FireObservation.class': 'cbb7b4a39a6ee5f200504142b91c65eb2e0ec04b95998ef0c6c2791b5e50a900', 'FlintAndSteelObservation$Observer.class': '631f0d2ed988efde615376d009f0521952be70ef9c52b1938e5a2493cef53d9f', 'FlintAndSteelObservation$Profile.class': '37de0ef36b0e6da1597b9e06d22e2d38392cb7bc7d1eaae630d010f05ae0a4f3', 'FlintAndSteelObservation.class': '23d6031562414eca373c61591ba5325d417e09be6e92cd2ec9db6acc13f6df21', 'PlayerDamageWakeObservation$Guard.class': 'd8f4514d89f717720edc16f79176ed57bc486e36ecf253f3920f15a2c13ee84a', 'PlayerDamageWakeObservation$Observer.class': 'cdc6d8e926a1d52d305c08b75eb599fef548d36ca17cbaf5cef8ee575de76efc', 'PlayerDamageWakeObservation.class': 'afe16cd0c098beb59c453265d36082c66faea673673f481f0a375841ad000ad0', 'UprightPlantsWakeObservation$Observer.class': '4b12128a03e91ab030f8bf91c7af852d2c2f8a5be591b51c123649ef056649ce', 'UprightPlantsWakeObservation$Profile.class': '9c889ad9a7219a80cd12df0ccbacf45eb519131e99a1f3cc52747499964ca317', 'UprightPlantsWakeObservation.class': '69b3b13853db4d808e850e47a75288d3ead38ba2842cdbf0bd27f4aeddcfb76e'}
BINARIES,RECEIPT_KEYS,PROCESS_TIMEOUTS=F.BINARIES,F.RECEIPT_KEYS,F.PROCESS_TIMEOUTS
public_receipt,class_record,receipt=F.public_receipt,F.class_record,F.receipt
CLASS_IDENTITIES={'ye': (759, 'c2b8b1a3b04e577928d52784949d650a3ffeb7febc790398e223bb82ce0d6f26', 'gm', 'f645ff415cca90dd495aa46eeedeee0d248dace3a33c81608ad142b105bc748d')}
MODES=tuple(p['mode'] for p in D.declared())


def profiles():
    result=P.load(HERE/'profiles.json')
    P.require(P.exact(result,D.declared()),'fixed ordered Flint and Steel operands')
    return result


def sources():return {name:P.digest(HERE/name) for name in SOURCES}
def shared():return {name:P.digest(path) for name,path in SHARED.items()}


def class_pins():
    extra=P.load(HERE/'flint-method-pins.json')
    P.require(type(extra) is dict and set(extra)=={'archive_sha256','classes'} and extra['archive_sha256']==P.JAR and
              type(extra['classes']) is dict and set(extra['classes'])==set(CLASS_IDENTITIES),'closed Flint class inventory')
    for name,(size,digest,parent,structure) in CLASS_IDENTITIES.items():
        class_record(extra['classes'][name],name,size,digest,parent)
        P.require(G.structural_hash(extra['classes'][name])==structure,'complete unchanged original Flint class and Code')
    base=O.class_pins()
    return dict(archive=base['archive'],classes={**base['classes'],**extra['classes']})


def method_pins():
    result=O.method_pins()
    for name in CLASS_IDENTITIES:
        for method in class_pins()['classes'][name]['methods']:
            if method['code_length'] is not None:
                result[name+'.'+method['name']+method['descriptor']]={k:method[k] for k in ('code_length','code_sha256','code_attribute_sha256')}
    return result


def full_trace(value,source,kind,exception=False):
    F.F.trace(value)
    callback='b' if kind=='range' else 'e' if kind=='sky' else 'a'
    prefix=[] if exception else ['java.lang.Thread.getStackTrace','FlintAndSteelObservation$Observer.event']
    if exception and kind is not None:prefix.append('FlintAndSteelObservation$Observer.interrupt')
    if kind is not None:prefix.append('FlintAndSteelObservation$Observer.'+callback)
    expected=[*prefix,*source,'FlintAndSteelObservation.run','FlintAndSteelObservation.main']
    P.require(P.exact([f['class']+'.'+f['method'] for f in value],expected),'complete genuine Flint callback/Throwable frame order')
    for frame in value:
        file='Thread.java' if frame['class']=='java.lang.Thread' else 'FlintAndSteelObservation.java' if frame['class'].startswith('FlintAndSteelObservation') else 'SourceFile'
        P.require(frame['file']==file,'unchanged original or authored frame file')


def validate(rows):
    P.require(type(rows) is list and len(rows)==len(MODES),'complete ordered Flint row inventory')
    prepared=dict(scheduled=dict(initialized=False,sequence='0'*16),entity=dict(initialized=False,next_id=0),stats=dict(total=[],unsent=[],dirty=False))
    for row,profile in zip(rows,profiles()):
        P.require(type(row) is dict and set(row)=={'mode','prepared','prepared_class','receiver','before','after','result','exception',
                  'exception_message','exception_stack','events'} and row['mode']==profile['mode'],'closed ordered Flint row')
        P.require(P.exact(row['prepared'],profile) and P.exact(row['prepared_class'],prepared),'literal operands and unnormalized prepared source class state')
        expected=model(profile,prepared)
        P.require(P.exact(row['receiver'],dict(**{'class':'ye'},id=259)),'actual unchanged YE receiver')
        for phase in ('before','after'):
            P.require(P.exact(row[phase],dict(expected[phase],chunk_planes=planes(profile,phase=='after',expected))),
                      'independent complete Flint '+phase+' state and all five Chunk planes: '+profile['mode'])
        for key in ('result','exception','exception_message'):
            P.require(P.exact(row[key],expected[key]),'independent Flint outcome '+key+': '+profile['mode'])
        P.require(type(row['events']) is list and len(row['events'])==len(expected['events']),'ordered actual Flint effect count')
        for actual,prediction in zip(row['events'],expected['events']):
            P.require(type(actual) is dict and set(actual)==(set(prediction)-{'source_stack'})|{'stack_trace'} and
                      P.exact({k:v for k,v in actual.items() if k!='stack_trace'},{k:v for k,v in prediction.items() if k!='source_stack'}),
                      'independent ordered Flint parameters, entropy and partial effects: '+profile['mode'])
            full_trace(actual['stack_trace'],prediction['source_stack'],prediction['kind'])
        if expected['exception']=='NONE':P.require(P.exact(row['exception_stack'],[]),'no invented Flint Throwable')
        else:full_trace(row['exception_stack'],expected['exception_source_stack'],expected['events'][-1]['kind'] if expected['exception']=='java/lang/IllegalStateException' else None,True)
        after=expected['after']
        prepared=dict(scheduled={k:after['scheduled'][k] for k in ('initialized','sequence')},entity=dict(after['entity_class']),stats={k:after['stats'][k] for k in ('total','unsent','dirty')})


def validate_finite(value):
    P.require(type(value) is dict and set(value)=={'schema','scope','rows'} and value['schema']==SCHEMA and value['scope']==SCOPE,'closed original Flint envelope')
    validate(value['rows'])


def validate_provenance(proof,directory=HERE):
    directory=Path(directory)
    keys={'schema','status','archive_sha256','finite_sha256','capture_sha256','sources','shared','methods','helpers','binaries','class_count','rows_per_vm',
          'original','processes','inputs_unchanged','helpers_unchanged','class_names_unchanged','class_pins_unchanged'}
    P.require(type(proof) is dict and set(proof)==keys and proof['schema']==PROVENANCE and proof['status']=='PASS_ORIGINAL_CPU_COMPONENTS_ONLY' and
              proof['archive_sha256']==P.JAR and proof['finite_sha256']==P.digest(directory/'finite.json') and P.word(proof['capture_sha256'],64) and
              P.exact(proof['class_count'],len(class_pins()['classes'])) and P.exact(proof['rows_per_vm'],len(MODES)) and
              all(proof[k] is True for k in ('inputs_unchanged','helpers_unchanged','class_names_unchanged','class_pins_unchanged')),'closed Flint provenance and preservation')
    for key,names in (('sources',SOURCES),('shared',SHARED),('helpers',HELPERS),('binaries',BINARIES)):F.F.fingerprints(proof[key],names,'closed '+key+' inventory')
    P.require(P.exact(proof['sources'],sources()) and P.exact(proof['shared'],shared()) and P.exact(proof['methods'],method_pins()) and P.exact(proof['helpers'],HELPER_IDENTITIES) and
              proof['binaries']['client']==P.JAR and all(proof['binaries'][n]==h for n,h in P.LIBRARIES.items()),'actual unchanged Flint Code and source bindings')
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
    value=read_reference(directory);print('Original Flint and Steel packet:',len(MODES),'fixed inputs and bindings pass')
    return value


if __name__=='__main__':check()
