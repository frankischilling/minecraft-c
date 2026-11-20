"""Strict finite component rows and independent read-only source trace checks."""
import json
from collections import Counter


def require(value,message):
    if not value:
        raise ValueError(message)


def rows(text,prefix):
    return [json.loads(line[len(prefix)+1:]) for line in text.splitlines() if line.startswith(prefix+' ')]


def components(text):
    found=rows(text,'TORCH_PARTICLE_COMPONENT')
    require([r['profile'] for r in found]==list(range(6,36)),'30 ordered deterministic component rows')
    require(Counter(r['operation'] for r in found)=={'tick':12,'draw':10,'draw_null_nw':2,'brightness':6},'declared component operation coverage')
    require(Counter(r['exception'] for r in found)=={'none':24,'NullPointerException':5,'ArithmeticException':1},'actual component outcomes')
    for row in found:
        if row['nw'] is not None:
            count=0 if row['exception']!='none' else 48
            require(row['nw']['words']==count and row['nw']['vertices']==count//8 and row['nw']['source_vertices']==count//12,'real NW quad-to-triangle append sizes')
            require(len(row['packed'])==100,'active words and actual retained tail projection')
        else:
            require(row['packed']==[],'non-draw component has no fabricated NW evidence')
    require('TORCH_PARTICLE_GUARD source network=0 native=0 write=0 exec=0' in text.splitlines(),'actual source guards')
    marker='TORCH_PARTICLE_COMPLETE constructors=37 measured=36 components=30 own-unchanged-words=25165440 network-controls=2 native-controls=1 write-controls=1 exec-controls=1'
    require(text.splitlines()[-1]==marker,'complete full own-prior backing audit and five denial controls')
    return found


def compare(actual,expected):
    require(json.dumps(actual,sort_keys=True)==json.dumps(expected,sort_keys=True),'exact typed deterministic component state, brightness and all packed word rows')


def trace(events,code,pins,component_rows):
    require([r['seq'] for r in events]==list(range(len(events))),'continuous actual JDI events')
    for row in code:
        pin=next(m for m in pins['classes'][row['class']]['methods'] if m['name']==row['name'] and m['descriptor']==row['descriptor'])
        require(row['bytes']==pin['code_bytes'] and row['sha256']==pin['code_sha256'],'every live source Code matches original archive')
    groups={i:[r for r in events if r['profile']==i] for i in range(6,36)}
    exceptions=[];platform_exceptions=[];counts=Counter()
    for component in component_rows:
        index=component['profile'];selected=groups[index]
        entries=[r for r in selected if r['event']=='entry']
        owner='qu' if component['kind']=='flame' else 'xn'
        operation=component['operation']
        first=owner+'.w_()V@0' if operation=='tick' else owner+'.a(F)F@0' if operation=='brightness' else owner+'.a(Lnw;FFFFFF)V@0'
        require(entries and entries[0]['at']==first,'actual subtype component entry')
        errors=[]
        for error in (r for r in selected if r['event']=='exception'):
            if error['at'].startswith(('sun.security.','java.util.jar.')):
                require(index==18 and error['exception'] in ('java.security.cert.CertPathValidatorException','java.security.SignatureException') and
                        error['catch'] is not None and error['catch'].startswith(('sun.security.','java.util.jar.')) and
                        'sn.a(F)F@133' in error['frames'],'caught JDK verification exception during first actual Block bootstrap')
                platform_exceptions.append(error)
            else:
                errors.append(error)
        if component['exception']=='none':
            require(not errors,'successful component has no hidden source exception')
        else:
            require(len(errors)==1 and errors[0]['exception']=='java.lang.'+component['exception'],'one actual declared component exception')
            error=errors[0];exceptions.append({'profile':index,'at':error['at'],'exception':error['exception'],'frames':error['frames']})
            expected='xn.w_()V@60' if index==11 else 'xw.a(Lnw;FFFFFF)V@150' if index in (32,34) else 'sn.a(F)F@118'
            require(error['at']==expected,'actual precise arithmetic/null failure PC')
        if operation=='tick':
            movement=[r for r in entries if r['at']=='sn.b(DDD)V@0']
            require(len(movement)==(0 if index==11 else 1),'zero-life Smoke fails before actual mover')
            for row in movement:
                require(row['caller']==owner+'.w_()V@'+('59' if owner=='qu' else '90'),'real subtype movement call PC')
            counts['movement']+=len(movement)
        else:
            vertices=[r for r in entries if r['at']=='nw.a(DDDDD)V@0']
            require(len(vertices)==(4 if component['exception']=='none' and operation.startswith('draw') else 0),'four actual source packed-vertex calls per successful sprite')
            counts['vertices']+=len(vertices)
            superclass=[r for r in entries if r['at']=='xw.a(Lnw;FFFFFF)V@0']
            require(len(superclass)==(1 if operation.startswith('draw') else 0),'subtype scale prefix enters real XW draw')
            for row in superclass:
                require(row['caller']==owner+'.a(Lnw;FFFFFF)V@'+('46' if owner=='qu' else '61'),'actual sprite super call PC')
                require(row['particle']['scale']==component['after']['scale'],'subtype scale is stored before superclass including failures')
            light=[r for r in entries if r['at']=='sn.a(F)F@0']
            require(len(light)==1,'actual SN brightness even for null NW/world prefixes')
            counts['brightness_queries']+=len(light)
        counts['components']+=1
    require(counts=={'movement':11,'vertices':32,'brightness_queries':18,'components':30},'finite actual component call counts')
    require([r['exception'] for r in platform_exceptions]==['java.security.cert.CertPathValidatorException','java.security.SignatureException','java.security.SignatureException'],
            'three retained caught platform JAR-verification observations')
    return {'events':len(events),'live_code_methods':len(code),'component_calls':dict(counts),'failures':exceptions,
            'caught_platform_verification_exceptions':platform_exceptions,
            'tick_scope':'declared no-clip; actual SN mover and EQ bounds offsets observed',
            'target_writes':0,'debugger_game_invocations':0}
