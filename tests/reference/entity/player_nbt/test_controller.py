"""Authored text-protocol controls, never substitute Player/World observations."""

import importlib.util
from pathlib import Path
import unittest


HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("nbt_original_controller", HERE / "run_original.py")
driver = importlib.util.module_from_spec(spec)
spec.loader.exec_module(driver)


def player_rows(case="control", stage="after"):
    nodes = {}
    top = dict(driver.PLAYER_FIELDS)
    nodes["$"] = (10, str(len(top)))
    for name, kind in top.items():
        payload = "00000000" if kind == 5 else "0"
        nodes["$/" + name] = (kind, payload)
    for name, kind, count in (("Pos", 6, 3), ("Motion", 6, 3), ("Rotation", 5, 2)):
        nodes["$/" + name] = (9, f"{kind},{count}")
        for i in range(count):
            nodes[f"$/{name}/{i}"] = (kind, "0" * (8 if kind == 5 else 16))
    nodes["$/Inventory"] = (9, "0,0")
    return [f"TAG {case} {stage} {path} {kind} {value}"
            for path, (kind, value) in nodes.items()]


def check_tree(rows):
    trees = driver.validate_trees(rows)
    for nodes in trees.values():
        driver.schema(nodes)
    return trees


class OriginalControllerControls(unittest.TestCase):
    def test_complete_authored_source_schema(self):
        self.assertEqual(len(check_tree(player_rows())), 1)

    def test_missing_and_duplicate_tag(self):
        rows = player_rows()
        for changed in (rows[:-1], rows + rows[-1:]):
            with self.subTest(length=len(changed)), self.assertRaises(ValueError):
                check_tree(changed)

    def test_orphan_path_and_missing_container(self):
        for extra in ("TAG control after other 3 0", "TAG control after $/missing/child 3 0"):
            with self.subTest(extra=extra), self.assertRaises(ValueError):
                check_tree(player_rows() + [extra])

    def test_vector_element_type_or_length(self):
        for text in ("9 6,2", "9 5,3"):
            rows = [line.replace("$/Pos 9 6,3", "$/Pos " + text) for line in player_rows()]
            with self.subTest(text=text), self.assertRaises(ValueError):
                check_tree(rows)

    def test_scalar_width_and_kind(self):
        for wrong in ("Health 2 32768", "Health 3 0", "Health 2 -32769"):
            rows = [line.replace("Health 2 0", wrong) for line in player_rows()]
            with self.subTest(wrong=wrong), self.assertRaises(ValueError):
                check_tree(rows)

    def test_no_invented_named_player_id(self):
        rows = player_rows()
        rows[0] = rows[0].replace(" 10 16", " 10 17")
        rows.append("TAG control after $/id 8 0050006c0061007900650072")
        with self.assertRaises(ValueError):
            check_tree(rows)

    def test_partial_spawn_coordinate_rejected(self):
        rows = player_rows()
        rows[0] = rows[0].replace(" 10 16", " 10 17")
        rows.append("TAG control after $/SpawnX 3 0")
        with self.assertRaises(ValueError):
            check_tree(rows)

    def test_invalid_slot_writer_shape(self):
        rows = [line.replace("$/Inventory 9 0,0", "$/Inventory 9 10,1") for line in player_rows()]
        rows += ["TAG control after $/Inventory/0 10 4",
                 "TAG control after $/Inventory/0/Slot 1 50",
                 "TAG control after $/Inventory/0/id 2 1",
                 "TAG control after $/Inventory/0/Count 1 1",
                 "TAG control after $/Inventory/0/Damage 2 0"]
        with self.assertRaises(ValueError):
            check_tree(rows)

    def test_partial_checkpoint_does_not_count_as_prefix(self):
        target = "Original Player NBT sleep: 1 completed call\n"
        for text in ("SITE sleep gs a (Lnu;)V 93\nOriginal Player NBT prefix: 1 checkpoint\n",
                     "Original Player NBT prefix: 0 checkpoint\n"):
            with self.subTest(text=text), self.assertRaises(ValueError):
                driver.validate_prefix(text, target, {"classes": {}}, Path(__file__))


if __name__ == "__main__":
    unittest.main()
