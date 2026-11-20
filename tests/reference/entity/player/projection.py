"""Validate unchanged original constructor observations and read-only JDI frames."""
import json,struct
from pathlib import Path

def rows(path): return [json.loads(line) for line in path.read_text().splitlines()]
def normalized(value):
    if isinstance(value,list): return [normalized(item) for item in value]
    if not isinstance(value,dict): return value
    if "class" in value and ("oid" in value or "identity" in value):
        name=value["class"]
        if name.startswith("[L"): name=name[2:-1]+"[]"
        return {"class":name,**({"utf16":value["utf16"]} if "utf16" in value else {}),**({"length":value["length"]} if "length" in value else {})}
    return {key:normalized(item) for key,item in value.items()}
def f32(value): return struct.unpack(">f",struct.pack(">f",value))[0]
def fbits(value): return struct.pack(">f",value).hex()
def default(value):
    if value is None or value is False or value==0: return True
    return isinstance(value,dict) and len(value)==1 and next(iter(value)) in ("F","D","J") and set(next(iter(value.values())))=={"0"}
def advance(seed,steps):
    for unused in range(steps): seed=(seed*0x5deece66d+11)&((1<<48)-1)
    return seed
def replay(before):
    seed=int(before,16); draws=[]; states=[]
    for unused in range(4):
        seed=advance(seed,1); high=seed>>(48-26)
        seed=advance(seed,1); low=seed>>(48-27)
        draws.append(((high<<27)+low)/float(1<<53)); states.append(f"{seed:012x}")
    return draws,states


def validate(normal_path,debugger,pins):
    checkpoints=rows(debugger/"checkpoints.ndjson")
    code_pins=[row for row in checkpoints if row["event"]=="code-pin"]
    assert {(row["class"],row["method"],row["descriptor"]) for row in code_pins}=={
        ("gs","b","()V"),("gs","<init>","(Lfd;)V"),("ls","<init>","(Lfd;)V"),
        ("dc","<init>","(Lnet/minecraft/client/Minecraft;Lfd;Lgr;I)V")}
    assert len(code_pins)==4
    targets={row["case"]:row for row in rows(debugger/"target-observations.ndjson") if row["event"]=="case"}
    normal={row["case"]:row for row in rows(normal_path) if row["event"]=="case"}

    for pin in [row for row in checkpoints if row["event"]=="code-pin"]:
        method=next(method for method in pins["classes"][pin["class"]]["methods"] if method["name"]==pin["method"] and method["descriptor"]==pin["descriptor"])
        assert method["code_sha256"]==pin["sha256"] and method["code_length"]==pin["bytes"],pin

    expected_watcher={"size":2,"dirty":False,"records":[{"index":0,"type":0,"value":0,"dirty":True},{"index":16,"type":0,"value":0,"dirty":True}]}
    projected=[]
    for case in range(8):
        target=targets[case]
        events=[row for row in checkpoints if row.get("case")==case]
        assert len(events)==(9 if case<=6 else 8),(case,len(events))
        assert len({row["self"]["oid"] for row in events})==1
        assert len({row["private_random_seed"] for row in events})==1
        assert all(row["watcher"]==expected_watcher for row in events)
        assert all(len(row["fields"])==156 for row in events)
        draws,seeds=replay(target["math_seed_before"])
        assert [struct.pack(">d",value).hex() for value in draws]==target["draws"]
        assert seeds[-1]==target["math_seed_after"]
        formulas={"ls.aj":fbits(f32(draws[0]*f32(.9)+f32(.1))),"ls.G":fbits(f32(f32(draws[1]+1)*f32(.01))),
                  "ls.F":fbits(f32(f32(draws[2])*f32(12398))),"sn.aS":fbits(f32((draws[3]*3.1415927410125732)*2))}
        hook=events[0]
        assert hook["event"]=="watcher-hook" and hook["location"]["pc"]==17
        assert hook["math_seed48"]==target["math_seed_before"]
        assert all(default(value) for key,value in hook["fields"].items() if key.startswith(("ls.","gs.","dc.")))
        assert hook["fields"]["sn.aF"] is False and hook["inventory"] is None
        for pc,steps,field in ((108,0,"ls.aj"),(168,1,"ls.G"),(195,2,"ls.F"),(211,3,"sn.aS")):
            event=next(row for row in events if row["location"]["class"]=="ls" and row["location"]["pc"]==pc)
            assert event["math_seed48"]==seeds[steps]
            assert event["fields"][field]["F"]==formulas[field]
        living=next(row for row in events if row["location"]["class"]=="ls" and row["location"]["pc"]==217)
        assert living["fields"]["sn.bp"]=={"F":"3f000000"} and living["fields"]["ls.Y"]==10
        for row in events[5:]: assert row["math_seed48"]==target["math_seed_after"]
        last=events[-1]
        if case<6:
            assert last["event"]=="return" and last["location"]["pc"]==109
            assert normalized(last["fields"])==normalized(target["player"]["fields"])
            assert normalized(last["bounds"])==normalized(target["player"]["bounds"])
            assert normalized(last["inventory"])==normalized(target["player"]["inventory"])
            assert normalized(last["container"])==normalized(target["player"]["container"]["fields"])
            assert last["filters"]==target["player"]["filters"]
            assert last["slots"]==45 and last["container_alias"] and last["distinct_nonnull_filters"]==3
            def deterministic(player):
                result=normalized(player); result.pop("private_random_seed")
                for field in ("ls.aj","ls.G","ls.F"): result["fields"].pop(field)
                return result
            assert deterministic(target["player"])==deterministic(normal[case]["player"])
        else:
            assert last["event"]=="exception" and target["outcome"]=="NullPointerException"
            assert last["inventory"]["ix.d"]["oid"]==last["self"]["oid"]
            assert last["fields"]["gs.l"] is None and last["fields"]["sn.bA"] is None
            if case==6:
                assert last["location"]["class"]=="dc" and last["location"]["pc"]==103
                assert last["slots"]==45 and last["container_alias"] and last["distinct_nonnull_filters"]==3
                assert last["fields"]["gs.m"]==2 and last["fields"]["ls.Y"]==20
            else:
                assert last["location"]["class"]=="gs" and last["location"]["pc"]==68
                assert last["container"] is None and last["slots"]==-1 and last["distinct_nonnull_filters"]==0
                assert last["fields"]["gs.z"]==20 and last["fields"]["ls.Y"]==10
                assert last["fields"]["sn.aI"] is None and last["fields"]["sn.bf"]=={"F":"00000000"}
        clean=[]
        for row in events:
            out=normalized({key:value for key,value in row.items() if key not in ("record","private_random_seed","entity_counter","math_seed48")})
            out["same_inventory_backref"]=row["inventory"] is not None and row["inventory"]["ix.d"]["oid"]==row["self"]["oid"]
            out["source_math_fields"]={key:out["fields"].pop(key) for key in ("ls.aj","ls.G","ls.F")}
            if row["location"]["class"]=="gs" and row["location"]["pc"]==165 or row["location"]["class"]=="dc": pass
            else: out["source_math_fields"]["sn.aS_before_Player_position"]=out["fields"].pop("sn.aS")
            clean.append(out)
        projected.append({"case":case,"source_outcome":target["outcome"],"checkpoints":clean,
            "separate_math_evidence":{"before_internal_seed":target["math_seed_before"],"after_internal_seed":target["math_seed_after"],"actual_draws_double_bits":target["draws"],
                                      "source_formula_replay_float_bits":formulas}})


    assert len(checkpoints)==75 and len(projected)==8
    return {"cases":projected}, {"cases":8,"successful_returns":6,"actual_failed_receivers":2,"snapshots":71,"watcher_hooks":8,"JDI_Code_pins":4}
