"""Typed physical Source-only observation graphs; no native graph is invented."""
import re


def need(v, message):
    if not v: raise ValueError(message)


def integer(v, bits=32):
    need(type(v) is int and -(1 << (bits-1)) <= v < (1 << (bits-1)), "typed signed integer")


def alias(v, positive=False):
    need(type(v) is int and (1 if positive else 0) <= v <= 4096, "bounded actual identity alias")


def scalar(v, allowed=None):
    if v is None: return
    need(type(v) is dict and type(v.get("class")) is str, "actual scalar owner")
    kind=v["class"];alias(v["alias"],True)
    if allowed is not None:need(kind in allowed,"declared Source collection value class")
    fields={"xu":{"alias","class","xyz","time"},"qy":{"alias","class","xyz","block","time","order"},
            "wf":{"alias","class","xyz"},"st":{"alias","class","region"},
            "hl":{"alias","class","stack","defaults"},"java.lang.Object":{"alias","class"}}
    need(kind in fields and set(v)==fields[kind],"closed Source scalar fields")
    if "xyz" in v:
        need(type(v["xyz"]) is list and len(v["xyz"])==3,"coordinate triple")
        for n in v["xyz"]:integer(n)
    for key in ("time","order"):
        if key in v:integer(v[key],64)
    if "block" in v:integer(v["block"])
    if kind=="st":
        need(type(v["region"]) is list and len(v["region"])==7 and v["region"][0] in (0,1),"light region")
        for n in v["region"]:integer(n)
    if kind=="hl":
        for key in ("stack","defaults"):
            need(type(v[key]) is list and len(v[key])==3,"Item scalar fields")
            for n in v[key]:integer(n)


def array_list(v, allowed):
    need(type(v) is dict and set(v)=={"alias","backing","size","capacity","modCount","slots"},"closed ArrayList")
    alias(v["alias"],True);alias(v["backing"],True)
    for key in ("size","capacity","modCount"):integer(v[key])
    need(0<=v["size"]<=v["capacity"]<=1024 and type(v["slots"]) is list and
         len(v["slots"])==v["capacity"],"complete ArrayList backing")
    need(all(x is None for x in v["slots"][v["size"]:]),"actual unused backing slots are null")
    for x in v["slots"][:v["size"]]:
        need(x is not None,"nonnull actual collection value");scalar(x,allowed)


def hash_set(v, keys):
    need(type(v) is dict and set(v)=={"alias","map","table","size","threshold","modCount","roots","nodes"},"closed HashSet map")
    for key in ("alias","map"):alias(v[key],True)
    alias(v["table"])
    for key in ("size","threshold","modCount"):integer(v[key])
    need(type(v["roots"]) is list and type(v["nodes"]) is list and
         0<=v["size"]==len(v["nodes"])<=256 and len(v["roots"])<=1024,"complete HashMap")
    need((v["table"]==0)==(len(v["roots"])==0),"unpublished table identity")
    cap=len(v["roots"]);need(cap==0 or cap&(cap-1)==0,"actual table power of two")
    nodes={}
    for x in v["nodes"]:
        need(type(x) is dict and set(x) in (
            {"alias","class","hash","key","value","next"},
            {"alias","class","hash","key","value","next","tree"}),"closed hash node")
        need(x["class"] in ("java.util.HashMap$Node","java.util.HashMap$TreeNode"),"actual JDK node class")
        alias(x["alias"],True);need(x["alias"] not in nodes,"unique hash node");nodes[x["alias"]]=x
        integer(x["hash"]);alias(x["next"]);scalar(x["key"],keys);scalar(x["value"],{"java.lang.Object"})
        if "tree" in x:
            need(x["class"].endswith("$TreeNode") and type(x["tree"]) is list and len(x["tree"])==5 and
                 type(x["tree"][4]) is bool,"actual tree-node fields")
            for n in x["tree"][:4]:alias(n)
    seen=set()
    for bucket,head in enumerate(v["roots"]):
        alias(head)
        while head:
            need(head in nodes and head not in seen,"acyclic complete hash buckets")
            seen.add(head);node=nodes[head]
            need((node["hash"] & (cap-1))==bucket,"actual hash bucket")
            head=node["next"]
    need(seen==set(nodes),"all actual nodes reachable")


def tree_set(v):
    need(type(v) is dict and set(v)=={"alias","map","root","nodes","size","modCount"},"closed TreeSet")
    alias(v["alias"],True);alias(v["map"],True);alias(v["root"])
    integer(v["size"]);integer(v["modCount"])
    need(type(v["nodes"]) is list and 0<=v["size"]==len(v["nodes"])<=256,"complete TreeMap")
    nodes={}
    for n in v["nodes"]:
        need(type(n) is dict and set(n)=={"alias","key","value","left","right","parent","color"} and
             type(n["color"]) is bool,"closed TreeMap node")
        alias(n["alias"],True);need(n["alias"] not in nodes,"unique tree node");nodes[n["alias"]]=n
        for key in ("left","right","parent"):alias(n[key])
        scalar(n["key"],{"qy"});scalar(n["value"],{"java.lang.Object"})
    seen=set()
    def visit(key,parent):
        if key==0:return
        need(key in nodes and key not in seen and nodes[key]["parent"]==parent,"actual acyclic tree/parent")
        seen.add(key);visit(nodes[key]["left"],key);visit(nodes[key]["right"],key)
    visit(v["root"],0);need(seen==set(nodes),"all actual TreeMap nodes reachable")


def validate(row, world_roles):
    for side in ("before","after"):
        arrays=row["arrays_"+side]
        need(type(arrays) is list and len(arrays)==20*len(world_roles),"all five arrays/four Chunks")
        for ordinal,a in enumerate(arrays):
            need(type(a) is list and len(a)==6 and a[:3]==[ordinal//20,(ordinal%20)//5,ordinal%5],
                 "ordered array inventory")
            for n in a[:5]:integer(n)
            alias(a[3],True);need(a[4]==(32768,16384,16384,16384,256)[ordinal%5] and
                type(a[5]) is str and re.fullmatch("[0-9a-f]{64}",a[5]),"actual array length/hash")
        worlds=row["worlds_"+side]
        need(type(worlds) is list and len(worlds)==len(world_roles),"complete World observations")
        for role,w in enumerate(worlds):
            need(type(w) is dict and set(w)=={"role","alias","time","a","B","o","s","M","light","scheduled_tree","scheduled_hash","entities"} and
                type(w["role"]) is int and w["role"]==role and w["time"]==world_roles[role]["authored_nbt_time"],
                "actual World role/loaded time")
            alias(w["alias"],True);integer(w["time"],64);integer(w["M"])
            for key in ("a","B","o","s"):need(type(w[key]) is bool,"actual World flags")
            array_list(w["light"],{"st"});array_list(w["entities"],{"hl"})
            tree_set(w["scheduled_tree"]);hash_set(w["scheduled_hash"],{"qy"})
        array_list(row["history_"+side],{"xu"})
        hash_set(row["position_set_"+side],{"wf"})
    need(type(row["changes"]) is list,"sparse byte changes")
    for change in row["changes"]:
        need(type(change) is list and len(change)==6,"complete sparse byte delta")
        for n in change:integer(n)
        w,c,p,at,before,after=change
        need(0<=w<len(world_roles) and 0<=c<4 and 0<=p<5 and
             0<=at<(32768,16384,16384,16384,256)[p] and 0<=before<=255 and 0<=after<=255 and
             before!=after,"valid actual changed byte")
