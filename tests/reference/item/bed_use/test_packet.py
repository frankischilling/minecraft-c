"""Closed packet/comparator corruption controls; no JVM or native launch."""
import copy
import importlib.util
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import packet


class PacketControls(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.text = (HERE / "selected.txt").read_text(encoding="ascii")
        cls.lines, cls.records = packet.rows(cls.text)
        cls.events = packet.read_journal()

    def test_untouched_capture(self):
        self.assertEqual(len(packet.read_reference()), 72)
        self.assertEqual(len(packet.read_json(HERE / "source-pins.json")["classes"]), 15)

    def test_missing_duplicate_reordered_rows(self):
        variants = [self.lines[:-1], [self.lines[0], *self.lines],
                    [self.lines[1], self.lines[0], *self.lines[2:]]]
        for lines in variants:
            with self.subTest(count=len(lines)), self.assertRaises(ValueError):
                packet.rows("\n".join(lines) + "\n")

    def test_malformed_inputs(self):
        for kind in ("index", "name", "yaw", "duplicate-key"):
            with self.subTest(kind=kind):
                lines = list(self.lines)
                row = copy.deepcopy(self.records[0])
                if kind == "index":
                    row["index"] = False
                elif kind == "name":
                    row["name"] = "missing-original-row"
                elif kind == "yaw":
                    row["values"][4] = "ffffffff"
                line = json.dumps(row, separators=(",", ":"))
                if kind == "duplicate-key":
                    line = '{"index":0,' + line[1:]
                lines[0] = "BED_PLACEMENT\t" + line
                with self.assertRaises(ValueError):
                    packet.rows("\n".join(lines) + "\n")

    def test_missing_store_sequence_and_begin(self):
        for kind in ("store", "sequence", "begin"):
            with self.subTest(kind=kind):
                events = copy.deepcopy(self.events)
                if kind == "store":
                    next(e for e in events if e["event"] == "field-write")["at"] = packet.LP + "204"
                elif kind == "sequence":
                    events[-1]["seq"] += 1
                else:
                    events[0]["event"] = "end"
                with self.assertRaises(ValueError):
                    packet.check_events(events, self.records)

    def test_discarded_false_returns(self):
        for name in ("height_127", "bed_id_zero_first_false_head_true", "callback_head_already_equal"):
            with self.subTest(name=name):
                profile = next(r["index"] for r in self.records if r["name"] == name)
                events = copy.deepcopy(self.events)
                e = next(e for e in events if e["profile"] == profile and e["event"] == "return" and
                         e.get("caller") in (packet.LP + "167", packet.LP + "194") and e["value"] is False)
                e["value"] = True
                with self.assertRaises(ValueError):
                    packet.check_events(events, self.records)

    def test_shortcircuit_and_failure_prefix(self):
        for name in ("first_air_blocked", "second_support_false", "callback_fail_foot_notice", "null_stack"):
            with self.subTest(name=name):
                profile = next(r["index"] for r in self.records if r["name"] == name)
                events = copy.deepcopy(self.events)
                selected = [e for e in events if e["profile"] == profile]
                if name in ("first_air_blocked", "second_support_false"):
                    next(e for e in selected if e["event"] == "entry" and e.get("caller", "").startswith(packet.LP))["caller"] = packet.LP + "36"
                else:
                    for e in selected:
                        if e["event"] == "exception":
                            e["frames"] = [f for f in e["frames"] if not f.startswith(packet.LP)]
                with self.assertRaises(ValueError):
                    packet.check_events(events, self.records)

    def test_captured_file_and_provenance_corruption(self):
        with tempfile.TemporaryDirectory(prefix="bed-use-packet-") as temporary:
            directory = Path(temporary) / "packet"
            shutil.copytree(HERE, directory, ignore=shutil.ignore_patterns("__pycache__"))
            for name in ("selected.txt", "source-journal.json", "profiles.tsv"):
                path = directory / name
                original = path.read_bytes()
                path.write_bytes(original + b"\n")
                with self.subTest(name=name), self.assertRaises(ValueError):
                    packet.read_reference(directory)
                path.write_bytes(original)
            path = directory / "provenance.json"
            value = packet.read_json(path)
            for name, changed in (("original_fresh_vms", 1), ("jdi_source_writes", 1), ("input_unchanged", False)):
                altered = copy.deepcopy(value)
                altered[name] = changed
                path.write_text(json.dumps(altered), encoding="utf-8")
                with self.subTest(name=name), self.assertRaises(ValueError):
                    packet.read_reference(directory)
            path.write_text(json.dumps(value), encoding="utf-8")
            self.assertEqual(len(packet.read_reference(directory)), 72)

    def test_native_comparator_closed_fixture(self):
        spec = importlib.util.spec_from_file_location("bed_use_native_comparator", packet.ROOT / "tests/tools/item/test_bed_use.py")
        driver = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(driver)
        effects, policies = copy.deepcopy(self.records), []
        groups = packet.check_events(self.events, self.records)
        for index, (row, group) in enumerate(zip(effects, groups)):
            callback = row["exception"] == "BedPlacementObservation$DeclaredCallbackFailure"
            null = row["exception"] == "java.lang.NullPointerException"
            if callback:
                row["exception"] = "NATIVE_CALLBACK_REFUSAL"
            pc = (167 if row["message"] == "foot" else 194) if callback else \
                 (92 if row["name"] == "null_world" else 200 if row["name"] == "null_stack" else 23) if null else \
                 209 if row["result"] == "true" else 7 if row["name"].startswith("face_guard_") else 211
            returned = {int(e["caller"].rsplit("@",1)[1]):e["value"] for e in group if e["event"] == "return" and
                        e.get("caller") in (packet.LP+"167",packet.LP+"194")}
            policies.append({"index":index,"status":7 if callback else 9 if null else 0,
                "native_policy":callback,"source_exception":null,"source_error":False,"observer_refused":callback,
                "source_pc":pc,"foot_returned":167 in returned,"foot_value":returned.get(167,False),
                "head_returned":194 in returned,"head_value":returned.get(194,False)})
        self.assertEqual(driver.compare(self.records,effects,policies,self.events)["effect_rows"],72)
        lines=["16 achievements","151 recipes"]
        for row, policy in zip(effects,policies):
            lines.extend(("BED_PLACEMENT\t"+json.dumps(row),"BED_PLACEMENT_NATIVE\t"+json.dumps(policy)))
        lines.append("PASS: 72 Bed direct profiles and wrapper controls, 123 actual-owner checks")
        self.assertEqual(driver.parse(("\n".join(lines)+"\n").encode("ascii")),(effects,policies))
        for kind in ("count", "boolean-count", "omission", "policy", "discarded-boolean"):
            altered=copy.deepcopy(effects);changed=copy.deepcopy(policies)
            if kind=="count":altered[67]["count_after"]=98
            elif kind=="boolean-count":altered[0]["message"]=False
            elif kind=="omission":altered.pop()
            elif kind=="policy":changed[0]["status"]=False
            else:changed[51]["foot_value"]=True
            with self.subTest(kind=kind),self.assertRaises(ValueError):
                driver.compare(self.records,altered,changed,self.events)
        for altered in (lines[:-1],lines+["extra"],["wrong"]+lines[1:]):
            with self.assertRaises(ValueError):
                driver.parse(("\n".join(altered)+"\n").encode("ascii"))

    def test_historical_producer_and_helper_identity(self):
        with tempfile.TemporaryDirectory(prefix="bed-use-history-") as temporary:
            directory=Path(temporary)/"packet"
            shutil.copytree(HERE,directory,ignore=shutil.ignore_patterns("__pycache__"))
            path=directory/"provenance.json"
            original=packet.read_json(path)
            for kind in ("current-unexecuted-writer","other-historical-producer","historical-helper","jdk-binary","sdk-binary","binary-inventory"):
                value=copy.deepcopy(original)
                if kind=="current-unexecuted-writer":
                    self.assertNotEqual(value["executed_producer_sha256"]["capture.py"],value["current_reproduction_sha256"]["capture.py"])
                    value["executed_producer_sha256"]["capture.py"]=value["current_reproduction_sha256"]["capture.py"]
                elif kind=="other-historical-producer":
                    value["executed_producer_sha256"]["prepare_inputs.py"]="0"*64
                elif kind=="historical-helper":
                    value["helper_sha256"]["BedPlacementJdi.class"]="0"*64
                elif kind=="jdk-binary":
                    value["binary_input_sha256"]["jdk/bin/java.exe"]="0"*64
                elif kind=="sdk-binary":
                    value["binary_input_sha256"]["sdk/org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar"]="0"*64
                else:
                    value["binary_input_sha256"]["jdk/bin/not-captured.dll"]=value["binary_input_sha256"].pop("jdk/bin/zip.dll")
                path.write_text(json.dumps(value),encoding="utf-8")
                with self.subTest(kind=kind),self.assertRaises(ValueError):
                    packet.read_reference(directory)
            path.write_text(json.dumps(original),encoding="utf-8")
            self.assertEqual(len(packet.read_reference(directory)),72)


if __name__ == "__main__":
    unittest.main()
