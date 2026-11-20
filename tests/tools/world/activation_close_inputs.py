"""Deferred canonical serialized inputs; no IO/imported helper/native launch."""
MODES = ("error", "null-error")


def prepared(mode, saving):
    if mode not in MODES:
        raise ValueError("Unknown activation close-result mode")
    info, region = saving.seed("item-chest")
    return {"world/level.dat": info, "world/region/r.0.0.mcr": bytes(region),
            "world/preserve.bin": b"activation close unrelated data\0\xff"}
