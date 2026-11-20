"""Authored real-font/real-context RenderEngine contracts, without a JAR."""
import argparse
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--constructor",type=Path,required=True)
    parser.add_argument("--textures",type=Path,required=True)
    parser.add_argument("--work-dir",type=Path,required=True)
    args=parser.parse_args()
    ref=Path(__file__).resolve().parents[2]/"reference/client/render_engine/support.py"
    spec=importlib.util.spec_from_file_location("render_support",ref)
    if spec is None or spec.loader is None:raise ValueError("render support")
    s=importlib.util.module_from_spec(spec);spec.loader.exec_module(s)
    args.work_dir.mkdir(parents=True,exist_ok=True)
    work=Path(tempfile.mkdtemp(prefix="render-engine-",dir=args.work_dir.resolve()))
    constructor=args.constructor.resolve(strict=True);textures=args.textures.resolve(strict=True)
    results={}
    for label,command,validator,trace in [
        ("constructor",[constructor],s.constructor_rows,False),
        ("constructor-trace",[constructor,"--trace-image"],s.constructor_rows,True),
        ("textures",[textures],s.texture_rows,False),
        ("textures-trace",[textures,"--reference"],s.texture_rows,True)]:
        r=s.run(command,work,label);observed=validator(r,trace)
        results[label]={"checks":observed["checks"],"capture":r.capture_report}
    bad_c=s.run([constructor,"--bad"],work,"bad-constructor")
    bad_t=s.run([textures,"--bad"],work,"bad-textures")
    for r,message in [(bad_c,b"RenderEngine input: [--trace-image]"),(bad_t,b"RenderTexture input: [--reference]")]:
        s.require(r.returncode==2 and r.stdout==b"" and r.stderr.splitlines()==[message],"exact malformed CLI rejection")
    # Parser rejection cannot mistake crashes/diagnostics for a valid outcome.
    actual=s.run([constructor,"--trace-image"],work,"controller-constructor")
    device=s.run([textures,"--reference"],work,"controller-textures")
    controls=[(s.constructor_rows,True,1,actual.stdout,b""),
        (s.constructor_rows,True,139,actual.stdout,b"AddressSanitizer: crash\n"),
        (s.constructor_rows,True,0,actual.stdout,b"runtime error: undefined behavior\n"),
        (s.constructor_rows,True,0,actual.stdout+b"extra\n",b""),
        (s.constructor_rows,True,0,actual.stdout.replace(b"METRIC 2",b"METRIC 1",1),b""),
        (s.constructor_rows,True,0,actual.stdout.replace(b"PIXELS ",b"PIXEL ",1),b""),
        (s.constructor_rows,True,0,actual.stdout.replace(b"ffffffff",b"00000000",1),b""),
        (s.constructor_rows,True,0,b"",b""),
        (s.texture_rows,True,1,device.stdout,b""),
        (s.texture_rows,True,0,device.stdout,b"leak report\n"),
        (s.texture_rows,True,0,device.stdout.replace(b"REG null null 1 0",b"REG null return 1 0",1),b""),
        (s.texture_rows,True,0,device.stdout.replace(b"REG plain return 2 1",b"REG plain return 2 0",1),b""),
        (s.texture_rows,True,0,device.stdout.replace(b"TRANSFER plain",b"TRANSFER anaglyph",1),b""),
        (s.texture_rows,True,0,device.stdout+b"extra\n",b""),
        (s.texture_rows,True,0,device.stdout[:len(device.stdout)//2],b"")]
    for index,(validate,trace,code,stdout,stderr) in enumerate(controls):
        try:validate(subprocess.CompletedProcess([],code,stdout,stderr),trace)
        except (ValueError,IndexError):continue
        raise ValueError("accepted malformed RenderEngine trace "+str(index))
    report={"status":"PASS","profiles":results,"controller_checks":len(controls)+4,
        "argc":[bad_c.capture_report,bad_t.capture_report],"work_directory":str(work)}
    (work/"report.json").write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8",newline="\n")
    print(json.dumps(report))


if __name__=="__main__":main()
