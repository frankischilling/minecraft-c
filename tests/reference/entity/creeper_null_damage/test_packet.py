"""Java-free static inheritance, literal and prefix corruption controls."""
from copy import deepcopy
import importlib.util
import json
from pathlib import Path
import unittest
HERE=Path(__file__).resolve().parent
S=importlib.util.spec_from_file_location('creeper_static_audit',HERE/'audit.py');A=importlib.util.module_from_spec(S);S.loader.exec_module(A)
class Packet(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.pins=A.load('source-pins.json')
 def reject(self,change):
  value=deepcopy(self.pins);change(value)
  with self.assertRaises(ValueError):A.validate(value)
 def test_public(self):self.assertEqual(A.audit_public()['executed_original_damage_calls'],0)
 def test_duplicate_JSON(self):
  with self.assertRaises(ValueError):json.loads('{"scope":1,"scope":2}',object_pairs_hook=A.unique)
 def test_runtime_claim(self):self.reject(lambda p:p.update(runtime_observation=True))
 def test_executed_calls(self):self.reject(lambda p:p.update(executed_original_damage_calls=1))
 def test_bool_call_count(self):self.reject(lambda p:p.update(executed_original_damage_calls=False))
 def test_wrong_archive(self):self.reject(lambda p:p.update(jar_sha256='0'*64))
 def test_wrong_class(self):self.reject(lambda p:p['classes']['gb'].update(class_sha256=p['classes']['gz']['class_sha256']))
 def test_wrong_super(self):self.reject(lambda p:p['classes']['gz'].update(superclass='wd'))
 def test_missing_II(self):self.reject(lambda p:p['classes'].pop('ii'))
 def test_inventory_omission(self):self.reject(lambda p:p['classes']['gb']['method_inventory'].pop())
 def test_invent_damage_override(self):self.reject(lambda p:p['classes']['gb']['method_inventory'].append({'name':'a','descriptor':'(Lsn;I)Z','access':1}))
 def test_parent_resolution(self):self.reject(lambda p:p['resolved_declarations'].update({'gz.b(Lsn;)V':'gz'}))
 def test_damage_resolution(self):self.reject(lambda p:p['resolved_declarations'].update({'gb.a(Lsn;I)Z':'ls'}))
 def test_hurt_literal(self):self.reject(lambda p:p['selected_references']['gb.j_()Ljava/lang/String;:0']['reference'].update(text='mob.creeperhurt'))
 def test_death_literal(self):self.reject(lambda p:p['selected_references']['gb.i()Ljava/lang/String;:0']['reference'].update(text='mob.creeper'))
 def test_literal_utf8(self):self.reject(lambda p:p['selected_references']['gb.j_()Ljava/lang/String;:0']['reference'].update(utf8_hex='00'))
 def test_parent_is_special(self):self.reject(lambda p:p['selected_references']['gb.b(Lsn;)V:2'].update(operation='invokevirtual'))
 def test_parent_symbolic_owner(self):self.reject(lambda p:p['selected_references']['gb.b(Lsn;)V:2']['reference'].update(**{'class':'ls'}))
 def test_guard_moved_before_parent(self):self.reject(lambda p:p['selected_instruction_windows']['gb.b(Lsn;)V'].reverse())
 def test_null_attacker_not_substituted(self):self.reject(lambda p:p['selected_instruction_windows']['ls.U()V'][1].update(opcode=43))
 def test_damage_amount(self):self.reject(lambda p:p['selected_instruction_windows']['ls.U()V'][2].update(opcode=5))
 def test_fatal_virtual_dispatch(self):self.reject(lambda p:p['selected_references']['ls.a(Lsn;I)Z:316']['reference'].update(**{'class':'gb'}))
 def test_inherited_volume(self):self.reject(lambda p:p['selected_instruction_windows']['ls.k()F'][0].update(opcode=11))

if __name__=='__main__':unittest.main()
