"""Authored fail-closed Player statistics projection/controller controls."""
import argparse
import importlib.util
from pathlib import Path
import sys
sys.dont_write_bytecode = True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_args()
    path = Path(__file__).resolve().parents[2] / "reference/client/player_statistics/support.py"
    spec = importlib.util.spec_from_file_location("mc_player_statistics_controls", path)
    if spec is None or spec.loader is None:
        raise RuntimeError("Player statistics support unavailable")
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    prefix = b"16 achievements\n151 recipes\n" * 4 + support.EFFECT + b"\n"
    rows = [b"STAT\t" + label.encode("ascii") + b"\tOK" + b"\t0" * 13 for label in support.LABELS]
    valid = prefix + b"\n".join(rows) + b"\nPlayer statistics: 1 checks\n"
    selected, count = support.rows(valid, native=True)
    support.require(selected == rows and count == 1, "positive finite projection control")
    malformed = {
        "missing-row": prefix + b"\n".join(rows[1:]) + b"\nPlayer statistics: 1 checks\n",
        "wrong-label": valid.replace(b"STAT\tnull\t", b"STAT\tunknown\t", 1),
        "extra-row": prefix + b"\n".join(rows + rows[:1]) + b"\nPlayer statistics: 1 checks\n",
        "trailing-diagnostic": valid + b"runtime error: signed overflow\n",
        "pre-count-diagnostic": valid.replace(b"Player statistics:", b"AddressSanitizer\nPlayer statistics:"),
        "wrong-count": valid.replace(b": 1 checks", b": 0 checks"),
        "missing-count": valid.rsplit(b"Player statistics:", 1)[0],
        "changed-banner": valid.replace(b"16 achievements", b"17 achievements", 1),
        "wrong-field-count": valid.replace(b"STAT\tnull\tOK\t0", b"STAT\tnull\tOK", 1),
        "non-ascii-field": valid.replace(b"STAT\tnull\tOK\t0", b"STAT\tnull\tOK\t\xff", 1),
        "unknown-outcome": valid.replace(b"STAT\tnull\tOK", b"STAT\tnull\tSUCCESS", 1),
        "fault-target-rows": valid,
        "missing-effect": valid.replace(support.EFFECT + b"\n", b"", 1),
        "changed-effect": valid.replace(b"EFFECT\twater\t8\t205", b"EFFECT\twater\t8\t237", 1),
    }
    for name, payload in malformed.items():
        try:
            support.rows(payload, native=True, fault=name == "fault-target-rows")
        except (ValueError, IndexError):
            pass
        else:
            raise RuntimeError("accepted malformed projection: " + name)
    original = b"16 achievements\n151 recipes\n" + support.EFFECT + b"\n" + b"\n".join(rows) + b"\nPLAYER_STATISTICS_ORIGINAL clean\n"
    support.require(support.rows(original, native=False)[0] == rows, "actual original completion marker")
    support.require(support.rows(b"16 achievements\n151 recipes\n" + support.EFFECT + b"\nPlayer statistics: 1 checks\n", native=True, fault=True)[1] == 1,
                    "positive separate native fault completion")
    print("Player statistics controller: 17 controls")


if __name__ == "__main__":
    main()
