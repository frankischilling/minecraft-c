"""Exact existing producers, before Source bootstrap; no IO on import."""
MODES = ("preflight", "success", "absent", "math-missing", "math-limit")


def prepared(mode, saving, construction):
    if mode not in MODES:
        raise ValueError("Closed single-Entity insertion mode")
    info, region = saving.seed("empty")
    return {"world/level.dat": bytes(info),
            "world/data/idcounts.dat": construction.raw_ids(),
            "world/region/r.0.0.mcr": bytes(region),
            "world/preserve.bin": b"owned single-Entity insertion literal\0\xff"}
