"""Authored text-protocol controls, never runtime Entity/World fixtures."""
import importlib.util
from pathlib import Path
import subprocess
import sys
import unittest

spec = importlib.util.spec_from_file_location("world_tick_driver", Path(__file__).with_name("run.py"))
driver = importlib.util.module_from_spec(spec)
spec.loader.exec_module(driver)


def authored_protocol():
    result=[]
    events={15:[0],16:[0],17:[0,0],18:[0],19:[0,1]}
    for scene in [s for s in range(24) if s != 7]:
        for i,identity in enumerate(events.get(scene,[])):
            result.append(f"EVENT {scene} {i} R {identity}")
        result.append(" ".join(["STATE",str(scene),"0" if scene==18 else "1"]+["0"]*30))
    result += [" ".join(["RIDE",str(scene)]+["0"]*16) for scene in range(7)]
    for scene in range(7):
        if scene in (1,2,3,4,5):
            result += [" ".join(["TP",str(scene),str(event),name]+["0"]*3) for event,name in enumerate(("smoke","flame"))]
        result.append(" ".join(["TILE",str(scene),"0" if scene==6 else "1"]+["0"]*8))
    return result


class ControllerControls(unittest.TestCase):
    def test_complete_authored_schema(self):
        rows = authored_protocol()
        self.assertEqual(driver.projection("\n".join(rows)), rows)

    def test_missing_duplicate_and_wrong_order(self):
        rows = authored_protocol()
        for changed in (rows[:-1], rows + rows[-1:], rows[1:] + rows[:1]):
            with self.subTest(changed=changed[:1]), self.assertRaises(ValueError):
                driver.projection("\n".join(changed))

    def test_completion_inventory(self):
        rows = authored_protocol()
        rows[0] = rows[0].replace("STATE 0 1", "STATE 0 0")
        with self.assertRaises(ValueError):
            driver.projection("\n".join(rows))

    def test_partial_and_wrong_event(self):
        for kind in ("partial","identity","index"):
            rows=authored_protocol()
            i=next(i for i,line in enumerate(rows) if line.startswith("EVENT "))
            if kind=="partial": rows[i]=rows[i].rsplit(" ",1)[0]
            elif kind=="identity": rows[i]=rows[i][:-1]+"2"
            else: rows[i]=rows[i].replace("EVENT 15 0","EVENT 15 1")
            with self.subTest(kind=kind),self.assertRaises(ValueError):
                driver.projection("\n".join(rows))

    def test_partial_state(self):
        rows = authored_protocol()
        rows[0] = rows[0].rsplit(" ", 1)[0]
        with self.assertRaises(ValueError):
            driver.projection("\n".join(rows))

    def test_partial_riding_tile_and_particle(self):
        for prefix in ("RIDE ","TILE ","TP "):
            rows=authored_protocol();i=next(i for i,line in enumerate(rows) if line.startswith(prefix))
            rows[i]=rows[i].rsplit(" ",1)[0]
            with self.subTest(prefix=prefix),self.assertRaises(ValueError):
                driver.projection("\n".join(rows))

    def test_tile_completion_and_particle_order(self):
        for kind in ("completion","name","ordinal"):
            rows=authored_protocol()
            if kind=="completion":
                i=next(i for i,line in enumerate(rows) if line.startswith("TILE 6 "))
                rows[i]=rows[i].replace("TILE 6 0","TILE 6 1")
            else:
                i=next(i for i,line in enumerate(rows) if line.startswith("TP "))
                rows[i]=rows[i].replace("smoke","flame") if kind=="name" else rows[i].replace("TP 1 0","TP 1 1")
            with self.subTest(kind=kind),self.assertRaises(ValueError):
                driver.projection("\n".join(rows))

    def test_positive_final_native_marker(self):
        self.assertEqual(driver.native_count("diagnostic\nWorld Entity tick: 12 checks\n"), 12)
        for text in ("", "partial\n", "World Entity tick: 0 checks\n", "World Entity tick: 1 checks\npartial\n"):
            with self.subTest(text=text), self.assertRaises(ValueError):
                driver.native_count(text)

    def test_real_exit_and_stderr_override_marker(self):
        # Small outputs are explicitly bounded by the child program here; the
        # production driver's separately reviewed capture owns pipe fault tests.
        for body in ("print('World Entity tick: 1 checks');raise SystemExit(7)",
                     "import sys;print('World Entity tick: 1 checks');sys.stderr.write('diagnostic')"):
            result = subprocess.run([sys.executable, "-B", "-c", body], capture_output=True, timeout=5)
            with self.subTest(body=body), self.assertRaises(ValueError):
                driver.require_process(result)


if __name__ == "__main__":
    unittest.main()
