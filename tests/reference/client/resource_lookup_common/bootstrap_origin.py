"""Exact mature JDK nest-origin predicate; no metadata active use on import."""
import re
from protocol import need

def admit_with_metadata(name,source,previous,forbidden,classes):
    match=re.fullmatch(r'([^/]+)\$\$Lambda\$[0-9]+/0x[0-9a-fA-F]+',name)
    need(match is not None,'explicit JDK bootstrap/generated origin '+name)
    declaring=match.group(1)
    need(declaring not in forbidden and source not in forbidden and name not in forbidden,
         'no game/SDK/helper declaring or source host')
    need(declaring in classes and source in classes,'exact selected JDK declaring/source class proof')
    declared=classes[declaring]; host=classes[source]
    need(source==declaring or declared['NestHost']==source,'source is self or exact classfile NestHost; no prefix inference')
    if source!=declaring:
        need(declaring in host['NestMembers'],'exact reciprocal JDK nest membership')
    for actual_name,proof in [(declaring,declared),(source,host)]:
        need(proof['JMOD_class_identical'] is True,'actual lib/modules/JMOD selected class bytes identical')
        need(actual_name in previous,'actual earlier declaring and source host events')
        origin=previous[actual_name]
        need(type(origin) is str and origin.startswith(('jrt:/','shared objects file')),
             'both actual origins JRT/shared bootstrap; no file/unknown/VM-defined')
        if origin.startswith('jrt:/'):
            need(origin=='jrt:/'+proof['module'],'exact actual JRT module identity')
    return {'generated_class':name,'actual_declaring_class':declaring,'actual_source_host':source,
            'earlier_declaring_origin':previous[declaring],'earlier_host_origin':previous[source],
            'declaring_class_sha256':declared['actual_modules_class']['sha256'],
            'source_host_class_sha256':host['actual_modules_class']['sha256'],
            'source_relation':'same declaring' if source==declaring else 'exact NestHost plus reciprocal NestMembers',
            'actual_modules_JMOD_bytes_crosschecked':True}
