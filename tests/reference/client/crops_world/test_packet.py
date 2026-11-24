"""Integrity and semantic corruption controls for executed-original Crops rows."""
import copy
import hashlib
import json
from pathlib import Path
import shutil
import tempfile
import unittest

from packet import FILES,HERE,lines,projected_cases,read_packet,validate_events,validate_rows


class PacketTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.packet=read_packet()

    def copy_packet(self):
        directory=tempfile.TemporaryDirectory();self.addCleanup(directory.cleanup)
        root=Path(directory.name)
        for name in (*FILES,'provenance.json'):
            shutil.copyfile(HERE/name,root/name)
        return root

    def reject_event(self,select,change,message):
        events=copy.deepcopy(self.packet['events']);change(next(e for e in events if select(e)))
        with self.assertRaisesRegex(ValueError,message):
            validate_events(events,self.packet['observed'],self.packet['pins'],self.packet['live'])

    def test_complete(self):
        self.assertEqual((len(self.packet['rows']),len(self.packet['inputs'])),(44,44))
        self.assertEqual(sum(r['delta'] for r in self.packet['observed']),15744)
        self.assertEqual(sum(e['event']=='exception' for e in self.packet['events']),3)

    def test_projection(self):
        for place,xyz in [(1,[8,64,8]),(2,[3,65,3])]:
            cases=projected_cases(place)
            self.assertEqual([r['metadata'] for r in cases],[0,7,8,15])
            self.assertTrue(all(r['id']==59 and r['xyz']==xyz and r['vertices']==48 and len(r['words'])==384 for r in cases))

    def test_changed_product(self):
        root=self.copy_packet();p=root/'selected.txt';p.write_bytes(p.read_bytes().replace(b'metadata-0',b'metadata-1',1))
        with self.assertRaisesRegex(ValueError,'changed source/product'):read_packet(root)

    def test_rebound_frozen_profile(self):
        root=self.copy_packet();p=root/'profiles.txt';p.write_bytes(p.read_bytes().replace(b'metadata-0 0 ',b'metadata-0 1 ',1))
        data=json.loads((root/'provenance.json').read_text());pin=hashlib.sha256(p.read_bytes()).hexdigest()
        data['files_sha256'][p.name]=pin;data['frozen_authored_files_sha256'][p.name]=pin
        (root/'provenance.json').write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError,'frozen source/product'):read_packet(root)

    def test_boolean_count(self):
        root=self.copy_packet();data=json.loads((root/'provenance.json').read_text());data['profiles']=True
        (root/'provenance.json').write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError,'unchanged executed'):read_packet(root)

    def test_source_write(self):
        root=self.copy_packet();data=json.loads((root/'provenance.json').read_text());data['source_writes_from_jdi']=1
        (root/'provenance.json').write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError,'unchanged executed'):read_packet(root)

    def test_provenance_counts(self):
        root=self.copy_packet();data=json.loads((root/'provenance.json').read_text())
        data['source_observations']['root_calls']=45
        (root/'provenance.json').write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError,'counts equal actual'):read_packet(root)

    def test_duplicate_json(self):
        root=self.copy_packet();p=root/'provenance.json';p.write_text(p.read_text().replace('"format": 1,','"format": 1, "format": 1,',1))
        with self.assertRaisesRegex(ValueError,'duplicate JSON'):read_packet(root)

    def test_missing_row(self):
        selected=[line for line in lines(HERE/'selected.txt') if not line.startswith('CROPS\t0\t')]
        with self.assertRaisesRegex(ValueError,'complete original roles'):validate_rows(selected,self.packet['profiles'])

    def test_live_code(self):
        live=list(self.packet['live']);row=live[0].split('\t');row[-1]='0'*64;live[0]='\t'.join(row)
        with self.assertRaisesRegex(ValueError,'live reached Code'):validate_events(self.packet['events'],self.packet['observed'],self.packet['pins'],live)

    def test_metadata_order(self):
        self.reject_event(lambda e:e['event']=='entry' and e['at'].startswith('ew.e('),lambda e:e.update(caller=e['caller'].replace('@40','@39')),'metadata follows color')

    def test_query_coordinates(self):
        self.reject_event(lambda e:e['event']=='entry' and e['at'].startswith('ew.e('),lambda e:e['args'].__setitem__(0,e['args'][0]+1),'EW owner and coordinates')

    def test_signed_texture_result(self):
        self.reject_event(lambda e:e['event']=='return' and e['at'].startswith('ni.a(II)I@'),lambda e:e.update(result=0),'signed NI iadd texture return')

    def test_texture_override_order(self):
        self.reject_event(lambda e:e['event']=='entry' and e['at'].startswith('ni.a(II)I@'),lambda e:e.update(caller=e['caller'].replace('@8','@24')),'texture getter before override')

    def test_emission_read(self):
        self.reject_event(lambda e:e['event']=='field-read' and e['field']=='uu.s',lambda e:e.update(element=1),'final emission array element')

    def test_brightness_return(self):
        self.reject_event(lambda e:e['event']=='return' and e['at'].startswith('ew.a(IIII)F@'),lambda e:e.update(result='00000000'),'brightness child return preserved')

    def test_large_y_widening(self):
        self.reject_event(lambda e:e['event']=='entry' and e['profile']==28 and e['at'].startswith('cv.b('),lambda e:e['args'].__setitem__(3,'4170000010000000'),'i2f/fsub/f2d')

    def test_vertex_site(self):
        self.reject_event(lambda e:e['event']=='entry' and e['at'].startswith('nw.a(DDDDD)V@'),lambda e:e.update(caller=e['caller'].replace('@145','@146')),'exact source vertex sites')

    def test_active_word(self):
        observed=copy.deepcopy(self.packet['observed']);observed[0]['words'][0]=f'{int(observed[0]["words"][0],16)^1:08x}'
        with self.assertRaisesRegex(ValueError,'active words derive'):validate_events(self.packet['events'],observed,self.packet['pins'],self.packet['live'])

    def test_null_prefix(self):
        self.reject_event(lambda e:e['event']=='exception',lambda e:e.update(at=e['at'].replace('@14','@15')),'original NULL prefix')


if __name__=='__main__':
    unittest.main()
