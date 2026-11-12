"""Authored JAR/JDK-free protocol controls; source package is UNRUN."""
import unittest
from pathlib import Path
from unittest import mock
import compare_stream as d
from stream_cases import CASES,authored_gzip
class Driver(unittest.TestCase):
    def test_valid_rows(self):
        values=['E','B:-128','S:32767','I:-2147483648','J:9223372036854775807','F:7fc01234','D:fff0000000000000','A:00ff','R:0001ff','T:00000041d800','L:99[]','L:1[B:3;B:-1;]','C:{0061=I:1;0062=T:004e;}']
        for v in values:
            for end in (b'\n',b'\r\n'):
                self.assertEqual(d.parse_native(b'RESULT\tOK\t'+v.encode()+b'\tOPEN'+end)[2],v)
    def test_malformed(self):
        bad=[b'',b'RESULT\tOK\tE\tOPEN',b'RESULT\tOK\tE\tOPEN\nextra\n',b'RESULT\tOK\tE\tOPEN\x00\n',b'RESULT\tOK\tE\tOPEN\v\n',b'RESULT\tPASS\tE\tOPEN\n',b'RESULT\tOK\tE\tWRONG\n',b'RESULT\tTHROW\tAddressSanitizer\tCLOSED\n',b'RESULT\tOK\tB:128\tOPEN\n',b'RESULT\tOK\tI:01\tOPEN\n',b'RESULT\tOK\tJ:-0\tOPEN\n',b'RESULT\tOK\tT:0\tOPEN\n',b'RESULT\tOK\tA:f\tOPEN\n',b'RESULT\tOK\tF:7fc0123\tOPEN\n',b'RESULT\tOK\tL:128[]\tOPEN\n',b'RESULT\tOK\tL:1[B:1]\tOPEN\n',b'RESULT\tOK\tC:{0062=E;0061=E;}\tOPEN\n',b'RESULT\tOK\tC:{0061=E;0061=E;}\tOPEN\n',b'RESULT\tOK\tEX\tOPEN\n']
        for row in bad:
            with self.subTest(row=row),self.assertRaises((RuntimeError,UnicodeError)):d.parse_native(row)
    def test_depth(self):
        with self.assertRaises(RuntimeError):d.validate_value('L:1['*257+'E'+';]'*257)
    def test_original_inventory(self):
        jar=Path.cwd()/'authored.jar';rows=('ORIGIN\tij\t'+jar.as_uri()+'\nORIGIN\tas\t'+jar.as_uri()+'\nRESULT\tOK\tC:{}\tCLOSED\nEND\t13\n').encode('ascii')
        self.assertEqual(d.parse_original(rows,jar)[2],'C:{}')
        for bad in (rows.replace(b'END\t13',b'END\t12'),rows.replace(b'ORIGIN\tij',b'ORIGIN\tpp'),rows+b'junk\n',rows.replace(b'RESULT',b'OTHER')):
            with self.assertRaises(RuntimeError):d.parse_original(bad,jar)
    def test_completion_not_crash(self):
        class Capture:
            @staticmethod
            def require_success(result,empty_stderr):
                if result['exit']!=0 or not empty_stderr:raise RuntimeError('actual failure')
        for result in ({'exit':1},{'exit':-11},{'exit':0,'reader_errors':{'stderr':'OSError'},'pipe_capture_complete':True},{'exit':0,'reader_errors':{},'pipe_capture_complete':False}):
            with self.assertRaises(RuntimeError):d.completed(Capture,result,Path.cwd(),'absent')
    def test_ambient_refused(self):
        for key in d.AMBIENT:
            with mock.patch.dict(d.os.environ,{key:'bad'}),self.assertRaises(RuntimeError):d.environment()
    def test_authored_inventory(self):
        names=[x[0] for x in CASES];self.assertEqual(len(CASES),35);self.assertEqual(len(names),len(set(names)))
        self.assertEqual(authored_gzip(b'','bad-header'),b'authored')
        with self.assertRaises(ValueError):authored_gzip(b'','unknown')
if __name__=='__main__':unittest.main()
