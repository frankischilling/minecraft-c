"""Six exact synthetic saved Worlds. This module never supplies RNG state."""
import hashlib
import json
from pathlib import Path
import shutil

HERE=Path(__file__).resolve().parent
ROLES=(("zero",0),("age100",100),("age101",101),("wrap-old",9223372036854775757),
       ("wrap100",-9223372036854775759),("wrap101",-9223372036854775758))


def audit(directory=HERE):
    directory=Path(directory)
    metadata=json.loads((directory/"world-inputs.json").read_text())
    if set(metadata)!=set(map(str,range(6))):raise ValueError("six exact synthetic roles")
    for role,(name,time) in enumerate(ROLES):
        item=metadata[str(role)]
        if item["name"]!=name or type(item["authored_nbt_time"]) is not int or item["authored_nbt_time"]!=time:
            raise ValueError("exact prelaunch Time input")
        if set(item["files"])!={"level.dat","session.lock","region/r.0.0.mcr"}:
            raise ValueError("closed synthetic World inputs")
        for relative,pin in item["files"].items():
            path=directory/"world-inputs"/name/relative
            if hashlib.sha256(path.read_bytes()).hexdigest()!=pin:raise ValueError("synthetic input byte pin")
    return metadata


def create(directory, roles=None, packet_directory=HERE):
    metadata=audit(packet_directory)
    selected=list(range(6)) if roles is None else list(roles)
    if any(type(x) is not int or x not in range(6) for x in selected) or len(set(selected))!=len(selected):
        raise ValueError("declared unique World role selection")
    directory=Path(directory)
    directory.mkdir(parents=True,exist_ok=False)
    for role in selected:
        name=metadata[str(role)]["name"]
        shutil.copytree(Path(packet_directory)/"world-inputs"/name,directory/name)
    return {str(role):metadata[str(role)] for role in selected}
