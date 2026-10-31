"""Run actual native owners with authored resources; no JDK or game assets."""
import argparse
import json
import os
from pathlib import Path
import sys
import tempfile
sys.dont_write_bytecode = True
from capture import capture, digest, ensure, require_success
from compare import records


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--native-probe",type=Path,required=True)
    p.add_argument("--work-dir",type=Path,required=True)
    a=p.parse_args()
    ensure(a.native_probe.is_file(),"configured native probe missing")
    a.work_dir.mkdir(parents=True,exist_ok=True)
    output=Path(tempfile.mkdtemp(prefix="native-notice-",dir=a.work_dir)).resolve()
    resources=output/"resources"
    (resources/"lang").mkdir(parents=True)
    (resources/"achievement").mkdir()
    (resources/"achievement/map.txt").write_bytes(b"")
    (resources/"lang/en_US.lang").write_bytes(
        b"achievement.openInventory=Inventory task\n"
        b"achievement.openInventory.desc=Open using %1$s.\n"
        b"achievement.mineWood=Wood task\n"
        b"achievement.mineWood.desc=Authored wood description\n"
        b"achievement.get=Task complete\n")
    (resources/"lang/stats_US.lang").write_bytes(
        b"stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
        b"stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n")
    source=Path(__file__).resolve().parent
    before={x.name:digest(x) for x in source.iterdir() if x.is_file()}
    executable_before=digest(a.native_probe)
    report={"status":"RUNNING","kind":"authored_native_notice","sources":before,
            "native_sha256":executable_before,"profiles":{}}
    try:
        for mode in ("geometry","callbacks"):
            run=capture([str(a.native_probe.resolve()),mode,str(resources)],output,mode,60,os.environ.copy())
            require_success(run,empty_stderr=True)
            rows=records(output/run["stdout_file"],mode)
            report["profiles"][mode]={"records":len(rows),"capture":run}
            if mode=="callbacks":
                def hx(text):return text.encode("utf-16-be").hex()
                inventory,wood,title=hx("Inventory task"),hx("Wood task"),hx("Task complete")
                expect={
                    "E":(inventory,hx("Open using E."),5242880,1),
                    "Y_NULL_W":(inventory,hx("Open using Y."),5242880,1),
                    "SETTINGS_REPLACED":(inventory,hx("Open using E."),5242880,1),
                    "HOLE":(inventory,hx("Open using null."),5242880,1),
                    "KEY_INDEX":(inventory,hx("Open using null."),5242880,1),
                    "NULL_SETTINGS":(inventory,hx("Open using null."),5242880,1),
                    "NULL_BINDING":(inventory,hx("Open using null."),5242880,1),
                    "NULL_CLIENT_PREFIX":(wood,hx("Open using null."),5242880,1),
                    "NULL_COMPLETE_PREFIX":(title,hx("Open using null."),5242880,1),
                    "COMPLETE":(title,wood,5242881,0)}
                actual=[x for x in rows if x.startswith("STATE ")]
                ensure(actual==["STATE "+name+" "+" ".join(map(str,values)) for name,values in expect.items()],
                       "authored notification contract differs")
                report["authored_notification_cases"]=len(expect)
                report["repeated_reminders_without_Registry_allocation_growth"]=1000
        ensure(before=={x.name:digest(x) for x in source.iterdir() if x.is_file()},"test sources changed")
        ensure(digest(a.native_probe)==executable_before,"native executable changed")
        report["status"]="PASS"
    except Exception as error:
        report.update(status="FAIL",error=f"{type(error).__name__}: {error}")
    (output/"result.json").write_bytes((json.dumps(report,indent=2)+"\n").encode())
    print(json.dumps({"status":report["status"],"result":str(output/"result.json"),
                      **({"error":report["error"]} if "error" in report else {})}))
    return 0 if report["status"]=="PASS" else 1


if __name__=="__main__":
    raise SystemExit(main())
