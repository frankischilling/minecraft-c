"""Strict offline admission of finite original stable Piston activation evidence."""
import hashlib
import json
import math
from pathlib import Path
import re
import struct

HERE=Path(__file__).resolve().parent
JAR='af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
PINNED={
    'authored-worlds.json':'fda2b3f14afdef17af84e2bf3a92ccc31ccf7b13012d0b820eb0448c8125fbab',
    'cases.json':'3bbfedc2eddb10d1d81bfc15fe5ee418ec323bfc331e0b43eff398ded47d35df',
    'ConstructorJson.java':'9ed002149ed9782c5deb4b833be9ed97ad2319ed6291af526b02baf58f62ceb3',
    'native-observations.json':'04bcd99725a3c07d8eb3454fd5a8db18f4e1a16b1f352ccf301fedd78d1a9b18',
    'observations.json':'1a7a26df39885b7faf38968c26d23647a4a30e84a861c69b8ffc943c038c98b5',
    'PistonActivationObservation.java':'031c22d90892b600613557cbd0238aaf37b546a1897f18c31cfa390f88b34b24',
    'profiles.tsv':'b1a3ac41691cdf838bc97f1a5ab10e31cf2b4176c5083d440b2ef34eb43ce843',
    'provenance.json':'42df01e8988c0801226069aaedde457e69ed48375f1dc3164f2071d4f27f00c9',
    'source-pins.json':'ecc4099ed1d4f0f79798715b8199c6cb0dd8f6a7909f6597543ab74de8efb3fc',
    'write_world.py':'b2b1a16da016f25850e90759a35b70b1b1d140f320c3e44e28be277bef335f2c',
}
FILES=set(PINNED)|{'packet.py','test_packet.py','README.md'}
SUMMARY={'processes':2,'cases_per_process':6,'phase_snapshots_per_process':48,
         'direct_activations_per_process':9,'direct_FD_g_per_process':33,
         'callbacks_per_process':94,'sound_callbacks_per_process':15,
         'classes':24,'declared_methods':563,'Code_methods':552,'source_files':18,
         'compiled_products':4}
EXCLUDED=['full World tick/timer/game loop','GL/pixels','original save','JDI/live Code',
          'complete internal call/query trace','native parity',
          'native Source-context/sound-failure/budget policy equivalence',
          'Random/clock/Source field repair','captured RNG injection']


def require(value,message):
    if not value:raise ValueError(message)


def unique(pairs):
    out={}
    for key,value in pairs:
        require(key not in out,'duplicate JSON key: '+key);out[key]=value
    return out


def read(path):return json.loads(Path(path).read_text(encoding='utf8'),object_pairs_hook=unique)
def pin(path):
    raw=Path(path).read_bytes();return {'bytes':len(raw),'sha256':hashlib.sha256(raw).hexdigest()}


def structural_projection(value):
    if type(value) is list:return [structural_projection(v) for v in value]
    if type(value) is dict:
        return {k:structural_projection(v) for k,v in value.items() if k!='world_random' and
                not(k=='pitch_bits' and value.get('kind')=='sound' and
                    value.get('name') in ('tile.piston.out','tile.piston.in'))}
    return value


def _state(v):
    require(v['world_class']=='fd' and v['world_time']==0 and v['world_name']=='Piston' and
            v['world_seed']==23 and v['remote'] is False and v['loaded_chunks']==1 and
            v['entity_count']==v['player_count']==v['weather_entities']==v['unloaded_entities']==0,
            'actual source constructor name, loaded metadata and bounded World')
    require(type(v['iterating']) is bool and type(v['suppression']) is bool and
            len(v['cells'])==18 and all(len(c)==5 and all(type(i) is int for i in c)
                                       for c in v['cells']),'typed cells/World flags')
    r=v['world_random']
    require(set(r)=={'seed','cached','gaussian_bits'} and type(r['seed']) is int and
            0<=r['seed']<1<<48 and type(r['cached']) is bool and
            re.fullmatch('[0-9a-f]{16}',r['gaussian_bits']),'retained actual Random bits')
    labels={}
    for tile in v['retained']:
        require(tile['class']=='uk' and re.fullmatch('T[0-9]+',tile['label']) and
                tile['world_same'] is True and len(tile['xyz'])==3 and
                all(type(x) is int for x in tile['xyz']) and type(tile['invalid']) is bool,
                'retained original UK roots and World aliases')
        require(tile['label'] not in labels,'unique retained identity root')
        labels[tile['label']]=tile
        p=tile['piston']
        require(set(p)=={'block','metadata','facing','extending','render_head','current_bits',
                         'previous_bits'} and all(type(p[k]) is int for k in
                         ('block','metadata','facing')) and
                all(type(p[k]) is bool for k in ('extending','render_head')) and
                all(re.fullmatch('[0-9a-f]{8}',p[k]) for k in
                    ('current_bits','previous_bits')),'exact actual UK fields')
    for tile in [*v['active'],*v['pending'],*[e['tile'] for e in v['map']]]:
        require(tile==labels[tile['label']],'same actual retained Tile field projection')
    require(set(v['chunk_array_sha256'])=={'Blocks','Data','SkyLight','BlockLight','HeightMap'} and
            all(re.fullmatch('[0-9a-f]{64}',s) for s in v['chunk_array_sha256'].values()),
            'all complete actual Chunk array hash observations')


def _rows(observation,cases,source):
    require(observation['origin']=={'jar_sha256':JAR,'classes':
            {n:p['class_sha256'] for n,p in source['classes'].items()}},
            'actual original loaded Class/resource owners')
    rows=observation['rows'];require(len(rows)==6 and [r['index'] for r in rows]==list(range(6)),
                                  'six ordered actual scenarios')
    for case,row in zip(cases,rows):
        i=case['index'];expected=['initial','activate-on']+(
            ['blocked-quiet-'+str(n) for n in range(1,4)] if case['blocked'] else
            ['extend-'+str(n) for n in range(1,4)]+['activate-off']+
            ['retract-'+str(n) for n in range(1,4)]+['quiet-1','quiet-2'])
        require(row['profile']==[str(i),case['folder'],case['name'],str(case['base']),
                str(case['count']),str(case['carried']),str(case['blocked']).lower()] and
                [s['stage'] for s in row['steps']]==expected,'exact predeclared inputs/entries')
        for n,step in enumerate(row['steps']):
            activation=step['stage'].startswith('activate')
            require(step['returned'] is True and step['failure'] is None and
                    (step['activation_result'] is True if activation else
                     step['activation_result'] is None),'actual returned direct source calls')
            require(step['operation']==('XR.a(FD,III,NULL)' if activation else 'observe' if n==0
                                        else 'FD.g()V'),'declared direct-call arity')
            _state(step['state'])
            require(type(step['event_start']) is int and type(step['event_end']) is int and
                    step['event_start']==(0 if n==0 else row['steps'][n-1]['event_end']) and
                    step['event_start']<=step['event_end']<=len(row['events']) and
                    all(e['stage']==step['stage'] for e in
                        row['events'][step['event_start']:step['event_end']]),
                    'complete contiguous per-call actual callback ranges')
        require(row['steps'][-1]['event_end']==len(row['events']),'complete actual callback suffix')
        for n,event in enumerate(row['events']):
            require(type(event['index']) is int and event['index']==n and event['stage'] in expected,
                    'ordered actual callback identity/stage')
            if event['kind']=='sound':
                require(len(event['position_bits'])==3 and all(re.fullmatch('[0-9a-f]{16}',b)
                        for b in event['position_bits']) and all(re.fullmatch('[0-9a-f]{8}',
                        event[k]) for k in ('volume_bits','pitch_bits')),'all raw sound arguments')
                require(all(math.isfinite(struct.unpack('>d',bytes.fromhex(b))[0]) for b in
                            event['position_bits']) and all(math.isfinite(struct.unpack('>f',
                            bytes.fromhex(event[k]))[0]) for k in ('volume_bits','pitch_bits')),
                        'finite actual sound arguments')
                _state(event['state'])
    footer=observation['footer']
    require(footer['rows']==6 and all(type(footer[k]) is int and footer[k]==0 for k in
            ('network','natives','execs','denied_writes','raw_allocations','source_field_repairs',
             'full_world_ticks')),'bounded original execution scope footer')
    require(sum(e=={'role':'owned-session-lock'} for e in footer['write_checks'])==6 and
            all(e['role'] in ('owned-session-lock','existing-owned-region-rw-open',
                             'existing-world-directory') for e in footer['write_checks']),
            'only owned lock writes/existing allowed opens')
    return rows


def _content(root):
    declaration=read(root/'cases.json');cases=declaration['cases']
    require(declaration['schema']=='ORIGINAL_PISTON_ACTIVATION_INPUTS_V1' and
            declaration['modes']==['interpreter','default'] and len(cases)==6 and
            [c['index'] for c in cases]==list(range(6)) and declaration['chunks']==[[0,0]] and
            declaration['constructor_inputs']=={'name':'Piston','seed':-99},'exact declared scope')
    profiles=''.join('%d\t%s\t%s\t%d\t%d\t%d\t%s\r\n'%(c['index'],c['folder'],c['name'],
            c['base'],c['count'],c['carried'],str(c['blocked']).lower()) for c in cases)
    require((root/'profiles.tsv').read_bytes()==profiles.encode('utf8'),'exact executed TSV bytes')
    source=read(root/'source-pins.json')
    require(source['schema']=='ORIGINAL_PISTON_ACTIVATION_CODE_PINS_V1' and
            source['jar_sha256']==JAR and len(source['classes'])==24 and
            sum(len(c['methods']) for c in source['classes'].values())==563 and
            sum('code_bytes' in m for c in source['classes'].values() for m in c['methods'])==552,
            'actual selected archive Class/Code census')
    provenance=read(root/'provenance.json')
    require(provenance['schema']=='ORIGINAL_PISTON_ACTIVATION_PROVENANCE_V1' and
            provenance['scope']=={'boundary':'Direct XR.a(FD,III,NULLPlayer) plus synchronous original descendants and declared FD.g Entity/Tile passes',
                                 'modes':['interpreter','default'],'excluded':EXCLUDED} and
            provenance['independent_physical_audit']==SUMMARY,'finite actual evidence boundary')
    require(provenance['runtime']['target_options']==['-Xint interpreter only','-Xverify:all',
            '-Xmx256m','-XX:-UsePerfData','-Djava.awt.headless=true','-Djava.security.manager=allow',
            '--add-opens=java.base/java.util=ALL-UNNAMED'] and
            provenance['runtime']['cleared_environment']==['JAVA_TOOL_OPTIONS','_JAVA_OPTIONS',
            'JDK_JAVA_OPTIONS'],'exact historical ordinary VM configuration')
    require(provenance['public_excludes']==['game JAR/classes/Code bytes','raw assets',
            'saved World/NBT/region files','raw private stdout/stderr/receipts',
            'private absolute paths','unobserved gameplay/native/pixel outcomes'],
            'exact public material/scope exclusions')
    require(set(provenance['streams'])=={'interpreter','default'},'both fresh ordinary processes')
    for stream in provenance['streams'].values():
        require(type(stream['exit']) is int and stream['exit']==0 and
                stream['timed_out'] is False and stream['pipe_capture_complete'] is True and
                stream['reader_errors']=={} and stream['output_overflow'] is False and
                stream['stdout_limit']==4194304 and stream['stderr_limit']==1048576,
                'complete original child transport')
    raw=read(root/'observations.json')
    require(raw['schema']=='ORIGINAL_PISTON_ACTIVATION_OBSERVATIONS_V1' and
            set(raw['modes'])=={'interpreter','default'},'both complete original raw projections')
    for mode in ('interpreter','default'):_rows(raw['modes'][mode],cases,source)
    a=structural_projection(raw['modes']['interpreter']);b=structural_projection(raw['modes']['default'])
    require(a==b,'actual two-mode structural equality with narrow raw entropy exclusion')
    require(sum(len(r['steps']) for r in a['rows'])==48 and
            sum(len(r['events']) for r in a['rows'])==94 and
            sum(e['kind']=='sound' for r in a['rows'] for e in r['events'])==15,
            'complete actual stage/callback census')
    native=read(root/'native-observations.json')
    require(native['schema']=='ORIGINAL_PISTON_ACTIVATION_STRUCTURAL_PROJECTION_V1' and
            native['rows']==a['rows'],'native expected table derives only from actual observations')
    authored=read(root/'authored-worlds.json')
    require(authored['schema']=='PISTON_ACTIVATION_AUTHORED_INPUTS_V1' and
            len(authored['files'])==18 and len(authored['directories'])==18 and
            len(authored['payloads'])==12,'complete authored compressed/payload input pins')
    return {'declaration':declaration,'cases':cases,'expected':a['rows']}


def read_packet(root=HERE):
    root=Path(root);require({p.name for p in root.iterdir()}==FILES|{'publication.json'} and
                           all(p.is_file() for p in root.iterdir()),'exact public packet census')
    manifest=read(root/'publication.json')
    require(manifest['schema']=='PISTON_ACTIVATION_PUBLICATION_V1' and
            manifest['kind']=='postexecution-finite-projection' and
            set(manifest['files'])==FILES and manifest['files']==
            {n:pin(root/n) for n in sorted(FILES)},'complete current public file hashes/lengths')
    value=_content(root)
    for name,digest in PINNED.items():require(pin(root/name)['sha256']==digest,
            'independently audited immutable original projection: '+name)
    return value


if __name__=='__main__':
    read_packet();print('PASS original Piston activation packet: 2 modes, 6 cases, 48 phases, 94 callbacks')
