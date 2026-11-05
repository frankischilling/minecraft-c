"""Generate only declared synthetic World inputs; never read a personal save."""
import argparse
import gzip
import importlib.util
from pathlib import Path
import struct

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
ROLES=(("zero",0),("age100",100),("age101",101),("wrap-old",9223372036854775757),
       ("wrap100",-9223372036854775759),("wrap101",-9223372036854775758))

def generate(output):
    output=Path(output);output.mkdir(parents=True,exist_ok=False)
    spec=importlib.util.spec_from_file_location("torch_synthetic_world",
        ROOT/"tests/reference/client/picking/observe.py")
    producer=importlib.util.module_from_spec(spec);spec.loader.exec_module(producer)
    for name,time in ROLES:
        directory=output/name;producer.world(directory)
        path=directory/"level.dat";wire=gzip.decompress(path.read_bytes())
        anchor=b"\x04\x00\x04Time"+bytes(8)
        if wire.count(anchor)!=1:raise ValueError("one authored Time payload")
        wire=wire.replace(anchor,b"\x04\x00\x04Time"+struct.pack(">q",time))
        path.write_bytes(gzip.compress(wire,mtime=0))
    return output

if __name__=="__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output",type=Path,required=True)
    generate(parser.parse_args().output)
