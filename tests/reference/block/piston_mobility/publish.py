"""Physically re-audit both retained original captures before public projection."""
import argparse
from pathlib import Path
import packet as P
C=P.C
def project(output,report):
    C.audit_private(output,report)  # Complete physical audit precedes every public write.
    P.declaration(P.parse((P.HERE/'cases.json').read_bytes()))
    pins={'jar_sha256':C.JAR,'classes':report['class_pins']};P.source(pins);P.finite(report['finite'],pins)
    streams={mode:(output/report[mode]['stdout_file']).read_bytes().decode('utf8') for mode in P.MODES}
    runs={}
    for mode in P.MODES:
        r=report[mode];runs[mode]={n:r[n] for n in ('exit','timeout_seconds','timed_out','pipe_capture_complete','reader_errors','output_overflow','stdout_bytes','stdout_sha256','stderr_bytes','stderr_sha256')}
        runs[mode].update(command=P.command(mode),stderr_scope='Exact four JDK17 SecurityManager warnings; raw retained privately and physically audited.')
    compile_record={n:report['compile'][n] for n in ('exit','timeout_seconds','timed_out','pipe_capture_complete','reader_errors','output_overflow','stdout_bytes','stdout_sha256','stderr_bytes','stderr_sha256')}
    compile_record['command']=['<javac17>','-proc:none','-encoding','UTF-8','-d','<compiled-observer>','PistonMobilityObservation.java']
    provenance={'schema':'PISTON_MOBILITY_PROVENANCE_V1','status':'CAPTURED_ORIGINAL','scope':'Two fresh ordinary original VMs; direct Block.h leaves only; no field writes or World.','excluded':P.EXCLUDED,'jar_sha256':C.JAR,'modes':list(P.MODES),
        'inputs_before':report['inputs_before'],'inputs_after':report['inputs_after'],'products_before':report['products_before'],'products_after':report['products_after'],'removed_environment':report['removed_environment'],'host':report['host'],
        'compile':compile_record,'runs':runs,'capture_report':C.pin(output/'capture.json'),'archive_inventory':{'classes':78,'declared_methods':808,'Code_methods':807,'mobility_Code_pins':6},
        'captured_helpers':{n:report['inputs_before']['helper/'+n] for n in ('PistonMobilityObservation.java','cases.json','capture.py')},
        'public_support_pins':{'tests/reference/client/controller/'+n:report['inputs_before']['support/'+n] for n in ('capture.py','classfile.py')}}
    return {'source-pins.json':P.encoded(pins),'finite.json':P.encoded(report['finite']),'observations.json':P.encoded(streams),'provenance.json':P.encoded(provenance),'native-observations.h':P.native_header(report['finite']['rows'])}
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--capture',type=Path,required=True);a=p.parse_args();output=a.capture.resolve(strict=True);report=P.parse((output/'capture.json').read_bytes())
    P.need(report.get('status')=='PASS','only physically complete final original cohort');projected=project(output,report)
    files={n:(P.HERE/n).read_bytes() for n in P.FILES if n not in projected};files.update(projected)
    manifest={'schema':'PISTON_MOBILITY_PUBLICATION_V1','scope':'Post-execution packaging; archive pins are not live-Code evidence.','files':{n:P.pin(raw) for n,raw in files.items()}};files['publication.json']=P.encoded(manifest)
    P.audit(files)
    for n,raw in projected.items():(P.HERE/n).write_bytes(raw)
    (P.HERE/'publication.json').write_bytes(files['publication.json'])
    print('PASS: both retained streams re-audited before bounded public packet writes')
if __name__=='__main__':main()
