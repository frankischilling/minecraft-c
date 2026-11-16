"""Verify a complete external capture before writing any review/public projection."""
import argparse
import json
from pathlib import Path
import sys
import zipfile
import os
sys.dont_write_bytecode=True
import capture as C
def require_commands(report,expected):
    C.C.ensure(set(expected)=={'compile','java-version','javac-version','interpreter','default'},'complete expected command cohort')
    for name,command in expected.items():
        C.C.ensure(isinstance(report[name]['command'],list) and all(type(v) is str for v in report[name]['command']) and report[name]['command']==command,'exact captured command '+name)
def expected_commands(report,directory,projection):
    pins=report['inputs_before']
    def one(predicate):
        found=[Path(name) for name,pin in pins.items() if predicate(name,pin)]
        C.C.ensure(len(found)==1,'one exact pinned command input')
        return found[0]
    java=Path(report['java-version']['command'][0])
    javac=Path(report['javac-version']['command'][0])
    C.C.ensure(str(java) in pins and str(javac) in pins and java.parent==javac.parent,'pinned same-JDK executables')
    jar=one(lambda n,p:p['sha256']==C.JAR_HASH)
    libraries=[one(lambda n,p:p['sha256']==sha) for sha in C.LIBRARIES.values()]
    source=one(lambda n,p:Path(n).name=='FurnaceReadback.java')
    classes=directory/'classes'
    cp=os.pathsep.join(map(str,[classes,jar,*libraries]))
    out={'compile':[str(javac),'-encoding','UTF-8','-implicit:none','-classpath',cp,'-d',str(classes),str(source)],
        'java-version':[str(java),'-version'],'javac-version':[str(javac),'-version']}
    for mode,flags in (('interpreter',['-Xint']),('default',[])):
        out[mode]=[str(java),*flags,'-Xverify:all','-Xmx256m','-XX:-UsePerfData','-Djava.awt.headless=true',
            '-Djava.security.manager=allow','-classpath',cp,'FurnaceReadback',str(jar),str(directory/mode/'saves'),
            'alpha','0','64','2',projection['metadata']['name']]
    return out,jar
def verify(directory):
    directory=Path(directory).resolve(strict=True)
    report=json.loads((directory/'capture.json').read_text(encoding='utf8'),object_pairs_hook=C.unique)
    C.C.ensure(report['status']=='PASS' and report['source_unchanged'] is True and report['inputs_unchanged'] is True and report['compiled_unchanged'] is True,'completed captured cohort')
    source=Path(report['source']).resolve(strict=True)
    C.C.ensure(C.inventory(source)==report['source_before']==report['source_after'] and len(report['source_before'])==13,'actual native source bytes unchanged')
    for name,pin in report['inputs_before'].items():
        path=Path(name)
        C.C.ensure(pin==report['inputs_after'][name]=={'bytes':path.stat().st_size,'sha256':C.C.digest(path)},'actual original/helper/JDK input bytes stable')
    products=sorted((directory/'classes').rglob('*.class'))
    C.C.ensure(products and all(p.name.startswith('FurnaceReadback') for p in products),'only authored observer products in actual classpath')
    actual={str(p.relative_to(directory)).replace('\\','/'):C.C.digest(p) for p in products}
    C.C.ensure(actual=={n.replace('\\','/'):v for n,v in report['compiled_before'].items()}=={n.replace('\\','/'):v for n,v in report['compiled_after'].items()},'actual compiled products stable')
    decoded=C.disk_projection(source)
    expected,jar=expected_commands(report,directory,decoded['selected_projection'])
    require_commands(report,expected)
    C.C.ensure(C.C.digest(jar)==C.JAR_HASH,'actual original JAR unchanged')
    with zipfile.ZipFile(jar) as archive: pins={name:C.I.inspect_class(archive.read(name+'.class')) for name in C.CORE}
    C.C.ensure(pins==report['class_pins'],'archive class and Code identities re-read')
    C.C.ensure(json.loads((directory/'independent-input.json').read_text(encoding='utf8'),object_pairs_hook=C.unique)==decoded,'independent original input re-decode')
    independent=directory/'independent-input.json'
    C.C.ensure(report['independent_input']=={'bytes':independent.stat().st_size,'sha256':C.C.digest(independent)},'retained independent projection fingerprint')
    rows={}
    for mode in ('compile','java-version','javac-version','interpreter','default'):
        receipt=report[mode]
        C.transport_record(directory,receipt)
        C.C.ensure(json.loads((directory/(mode+'.capture.json')).read_text(encoding='utf8'),object_pairs_hook=C.unique)==receipt,'retained receipt is exactly report receipt')
        if mode in ('interpreter','default'):
            C.C.ensure((directory/(mode+'.stderr')).read_text(encoding='utf8').splitlines()==C.expected_warnings(directory/'classes'),'exact original guard warnings')
            row,footer=C.read_output((directory/(mode+'.stdout')).read_bytes(),pins)
            C.check_row(row,decoded['selected_projection'])
            C.C.ensure(report['observations'][mode]=={'row':row,'footer':footer},'raw original stream exactly projects to report')
            inv=C.inventory(directory/mode/'saves')
            C.C.ensure(inv==report['copies_after'][mode]==report['all_copies_after'][mode] and set(inv)==set(report['source_before']) and
                inv['alpha/session.lock']['bytes']==8 and all(inv[n]==v for n,v in report['source_before'].items() if n!='alpha/session.lock'),'actual copies changed only owned SessionLock')
            rows[mode]={'row':row,'footer':footer}
    C.C.ensure(rows['interpreter']['row']==rows['default']['row'],'fresh original modes agree after independent input checks')
    return report,rows
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    report,rows=verify(args.capture)
    output=args.output.resolve()
    C.C.ensure(not output.exists(),'new output only')
    # All transport, physical input/product, both-stream parse, and copy checks precede first output write.
    output.mkdir()
    (output/'finite.json').write_text(json.dumps(rows,indent=2)+'\n',encoding='utf8')
    (output/'source-pins.json').write_text(json.dumps({'jar_sha256':C.JAR_HASH,'classes':report['class_pins'],
        'meaning':'Archive class/Code identities and loaded class resource identities only; no JDI or entered/live Code trace.'},indent=2)+'\n',encoding='utf8')
    print('PASS: verified two-file review projection written; not a complete publication packet')
if __name__=='__main__': main()
