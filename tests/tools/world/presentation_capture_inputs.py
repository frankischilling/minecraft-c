"""Declared literal inputs, prepared before bootstrap; importing performs no I/O."""

MODES = ("sound-error", "sound-null-error", "particle-error", "particle-null-error")


def prepared(mode, saving, construction):
    if mode not in MODES:
        raise ValueError("Declared detailed presentation capture mode")
    info, region = saving.seed("empty")
    region = bytes(region)
    ids = construction.raw_ids()
    if type(info) is not bytes or type(region) is not bytes or type(ids) is not bytes:
        raise ValueError("Immutable canonical pre-bootstrap bytes")
    return {"world/level.dat": info,
            "world/data/idcounts.dat": ids,
            "world/region/r.0.0.mcr": region,
            "world/preserve.bin": b"detailed presentation unrelated data\0\xff"}
