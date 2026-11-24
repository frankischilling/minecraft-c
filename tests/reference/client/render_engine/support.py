"""RenderEngine protocol validators and reviewed process-capture owner."""
import importlib.util
from pathlib import Path
import re


def require(value,message):
    if not value:raise ValueError(message)


def run(command,cwd,prefix,timeout=180,env=None):
    source=Path(__file__).resolve().parents[1]/"statistics/support.py"
    spec=importlib.util.spec_from_file_location("render_capture",source)
    require(spec is not None and spec.loader is not None,"capture owner")
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    result=module.run_bounded(command,cwd,prefix,timeout=timeout,env=env)
    require(result.capture_report.get("reader_errors")=={},"explicit capture reader success")
    return result


def constructor_rows(result,trace=False):
    require(result.returncode==0 and result.stderr==b"","constructor exit/diagnostics")
    require(0<len(result.stdout)<100000,"constructor trace extent")
    rows=result.stdout.splitlines();require(len(rows)==13+int(trace),"constructor row inventory")
    require(re.fullmatch(rb"FONT .+ [0-9]+\.[0-9]+\.[0-9]+",rows[0]) is not None,"native font/provider")
    image=re.fullmatch(rb"IMAGE 64 64 BLACK ([1-9][0-9]*)",rows[1]);require(image is not None and int(image[1])<512,"real nonempty monochrome image")
    metrics=[]
    for i in range(10):
        cells=rows[2+i].split();require(len(cells)==9 and cells[:2]==[b"METRIC",str(i).encode()] and all(re.fullmatch(rb"-?[0-9]+",x) for x in cells[2:]),"ordered native glyph metric")
        metrics.append(tuple(map(int,cells[2:])))
    pixels=None;cursor=12
    if trace:
        cells=rows[cursor].split();require(len(cells)==4097 and cells[0]==b"PIXELS" and all(re.fullmatch(rb"(?:ffffffff|ff000000)",x) for x in cells[1:]),"generated ARGB extent/values")
        pixels=bytes.fromhex(b"".join(cells[1:]).decode("ascii"));require(sum(x==b"ff000000" for x in cells[1:])==int(image[1]),"native black count")
        cursor+=1
    count=re.fullmatch(rb"RenderEngine constructor: ([1-9][0-9]*) checks",rows[cursor]);require(count is not None and cursor+1==len(rows),"positive complete constructor checks")
    return {"checks":int(count[1]),"metrics":metrics,"pixels":pixels,"font":rows[0].decode("utf-8")}


def texture_rows(result,reference=False):
    require(result.returncode==0 and result.stderr==b"","texture exit/diagnostics")
    require(0<len(result.stdout)<30000,"texture trace extent")
    rows=result.stdout.splitlines();require(rows,"texture rows")
    count=re.fullmatch(rb"RenderTexture ownership: ([1-9][0-9]*) checks",rows[-1]);require(count is not None,"positive complete texture checks")
    if not reference:
        require(len(rows)==2 and re.fullmatch(rb"REAL_DEBUG_CALLBACK [0-9]+",rows[0]) is not None,"actual-context control inventory")
        return {"checks":int(count[1]),"rows":rows[:-1]}
    cursor=0;selected=[]
    for label,ledger,maps,limit in [("null",1,0,1048576),("plain",2,1,16),("anaglyph",3,2,16),("null-settings",4,1,16)]:
        outcome="null" if label=="null" else "return"
        expected=("REG %s %s %d %d 0 1 0 %d 1 1 9728 9728"%(label,outcome,ledger,maps,limit)).encode()
        require(cursor<len(rows) and rows[cursor]==expected,"ordered texture/source prefix")
        selected.append(rows[cursor]);cursor+=1
        if label!="null":
            for kind in ["GPU","TRANSFER"]:
                require(cursor<len(rows) and re.fullmatch((kind+" "+label).encode()+rb"(?: [0-9a-f]{2}){16}",rows[cursor]) is not None,"actual device/transfer byte row")
                selected.append(rows[cursor]);cursor+=1
    require(cursor+1==len(rows),"complete reference texture inventory")
    return {"checks":int(count[1]),"rows":selected}
