"""Corruption and passive-read controls for the original explosion packet."""
import copy
import json
from pathlib import Path
import unittest
import audit
from compare import compare, draw_rows, tick_rows

HERE=Path(__file__).resolve().parent


class ExplosionPacket(unittest.TestCase):
    def setUp(self):
        self.fixture=json.loads((HERE/'finite.json').read_text())
        self.tick_text='\n'.join('EXPLOSION_TICK '+json.dumps(row) for row in self.fixture['ticks'])
        self.draw_text='\n'.join(f"EXPLOSION_DRAW profile={row['profile']} brightness={row['brightness']} words="+
                                  ','.join(row['words']) for row in self.fixture['draws'])

    def test_exact_rows_and_passive_reads(self):
        before=copy.deepcopy(self.fixture)
        compare(tick_rows(self.tick_text),self.fixture['ticks'])
        compare(draw_rows(self.draw_text),self.fixture['draws'])
        self.assertEqual(self.fixture,before)

    def test_all_vertex_words_and_brightness_are_compared(self):
        for profile,row in enumerate(self.fixture['draws']):
            for index in range(48):
                changed=copy.deepcopy(self.fixture['draws'])
                changed[profile]['words'][index]=f"{int(row['words'][index],16)^1:08x}"
                with self.subTest(profile=profile,word=index),self.assertRaises(ValueError):
                    compare(changed,self.fixture['draws'])
            changed=copy.deepcopy(self.fixture['draws']);changed[profile]['brightness']='3f000000'
            with self.assertRaises(ValueError):compare(changed,self.fixture['draws'])

    def test_each_tick_scalar_and_geometry_word_is_compared(self):
        for profile,row in enumerate(self.fixture['ticks']):
            for key,value in row.items():
                if type(value) in (bool,int):
                    changed=copy.deepcopy(self.fixture['ticks'])
                    changed[profile][key]=not value if type(value) is bool else value+1
                    with self.subTest(profile=profile,field=key),self.assertRaises(ValueError):
                        compare(changed,self.fixture['ticks'])
                elif type(value) is list:
                    for index,word in enumerate(value):
                        changed=copy.deepcopy(self.fixture['ticks'])
                        changed[profile][key][index]=f'{int(word,16)^1:016x}'
                        with self.subTest(profile=profile,field=key,index=index),self.assertRaises(ValueError):
                            compare(changed,self.fixture['ticks'])

    def test_missing_repeated_and_reordered_profiles(self):
        for parser,text in ((tick_rows,self.tick_text),(draw_rows,self.draw_text)):
            lines=text.splitlines()
            for changed in (lines[:-1],lines+lines[:1],list(reversed(lines))):
                with self.assertRaises(ValueError):parser('\n'.join(changed))

    def test_tick_types_schema_and_duplicate_keys(self):
        for changed in (self.tick_text.replace('"age": 1','"age": true',1),
                        self.tick_text.replace('"dead": false','"dead": 0',1),
                        self.tick_text.replace('"age": 1','"age": 2147483648',1),
                        self.tick_text.replace('"age": 1','"age": 1, "age": 1',1),
                        self.tick_text.replace('"age": 1','"age": 1, "extra": 0',1),
                        self.tick_text.replace('"age": 1, ','',1)):
            with self.assertRaises(ValueError):tick_rows(changed)

    def test_draw_word_spelling_and_span(self):
        line=self.draw_text.splitlines()[0]
        for changed in (line.rsplit(',',1)[0],line+',00000000',line.replace('words=','words=xyz,',1)):
            with self.assertRaises(ValueError):draw_rows(changed+'\n'+'\n'.join(self.draw_text.splitlines()[1:]))

    def test_original_constructor_entropy_audits_are_passive(self):
        data=json.loads((HERE/'constructor-evidence.json').read_text())
        pins=json.loads((HERE/'source-pins.json').read_text())
        before=copy.deepcopy(data)
        self.assertEqual(audit.ordinary(data['ordinary'])['constructors'],13)
        self.assertEqual(audit.debugger(data['debugger'],data['debugger_target'],pins)['constructors'],14)
        self.assertEqual(data,before)

    def test_observed_constructor_stream_and_code_corruptions(self):
        data=json.loads((HERE/'constructor-evidence.json').read_text())
        pins=json.loads((HERE/'source-pins.json').read_text())
        for key in ('math_values','own_values'):
            changed=copy.deepcopy(data['debugger']);changed[0][key][0]='3ff0000000000000' if key=='math_values' else '3f800000'
            with self.assertRaises(ValueError):audit.debugger(changed,data['debugger_target'],pins)
        for key in ('own_initial','own_after','code_sha256'):
            changed=copy.deepcopy(data['debugger']);changed[0][key]='0'*len(changed[0][key])
            with self.assertRaises(ValueError):audit.debugger(changed,data['debugger_target'],pins)
        changed=copy.deepcopy(data['debugger']);changed[0]['state']['lifetime']+=1
        with self.assertRaises(ValueError):audit.debugger(changed,data['debugger_target'],pins)
        changed=copy.deepcopy(data['ordinary']);changed[0]['math_after']='000000000000'
        with self.assertRaises(ValueError):audit.ordinary(changed)
        with self.assertRaises(ValueError):audit.debugger(data['debugger'][:-1],data['debugger_target'],pins)


if __name__=='__main__':
    unittest.main()
