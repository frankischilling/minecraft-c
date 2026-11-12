"""Authored data definitions only; importing never writes or executes anything."""
import gzip
import zlib
CASES=(
 ('empty-compound','as',bytes.fromhex('0a000000'),'normal'),
 ('root-end-ij','ij',bytes.fromhex('00'),'normal'),
 ('root-end-as','as',bytes.fromhex('00'),'normal'),
 ('byte','ij',bytes.fromhex('01000080'),'normal'),
 ('short','ij',bytes.fromhex('0200008123'),'normal'),
 ('int','ij',bytes.fromhex('03000012345678'),'normal'),
 ('long','ij',bytes.fromhex('0400000102030405060708'),'normal'),
 ('float-nan','ij',bytes.fromhex('0500007fc01234'),'normal'),
 ('double-nan','ij',bytes.fromhex('0600007ff8000012345678'),'normal'),
 ('short-int','ij',bytes.fromhex('03000012'),'normal'),
 ('short-long','ij',bytes.fromhex('040000010203'),'normal'),
 ('negative-array','ij',bytes.fromhex('070000ffffffff'),'normal'),
 ('short-array','ij',bytes.fromhex('0700000000000342'),'normal'),
 ('unknown-named','ij',bytes.fromhex('0b000178'),'normal'),
 ('unknown-list-empty','ij',bytes.fromhex('0900006300000000'),'normal'),
 ('unknown-list-negative','ij',bytes.fromhex('09000063ffffffff'),'normal'),
 ('unknown-list-positive','as',bytes.fromhex('0900006300000001'),'normal'),
 ('utf-accepted','ij',bytes.fromhex('080000000b00c080c181e08080eda080'),'normal'),
 ('utf-malformed','ij',bytes.fromhex('080000000441e08058'),'normal'),
 ('utf-truncated-wire','ij',bytes.fromhex('080000000341e0'),'normal'),
 ('utf-truncated-codepoint','ij',bytes.fromhex('080000000341e080'),'normal'),
 ('compound-prefix','as',bytes.fromhex('0a000001000161070b000162'),'normal'),
 ('crc-not-reached','as',bytes.fromhex('0a000000'),'bad-crc'),
 ('invalid-header','as',b'','bad-header'),
 ('truncated-header','as',b'','short-header'),
 ('header-extra-name-comment','as',bytes.fromhex('0a000000'),'optional-header'),
 ('reserved-header-flags','as',bytes.fromhex('0a000000'),'reserved-flags'),
 ('header-crc','as',bytes.fromhex('0a000000'),'header-crc'),
 ('bad-header-crc','as',bytes.fromhex('0a000000'),'bad-header-crc'),
 ('raw-empty','gzip',b'','normal'),
 ('raw-crc','gzip',b'authored payload','bad-crc'),
 ('raw-size','gzip',b'authored payload','bad-size'),
 ('raw-concatenated','gzip',b'first','concatenated'),
 ('raw-ignored-next-header','gzip',b'first','junk-tail'),
 ('raw-truncated-trailer','gzip',b'first','short-trailer'),
)
def authored_gzip(payload,variant):
    if variant=='bad-header':return b'authored'
    if variant=='short-header':return b'\x1f\x8b\x08'
    data=bytearray(gzip.compress(payload,mtime=0))
    if variant=='bad-crc':data[-8]^=1
    elif variant=='bad-size':data[-4]^=1
    elif variant=='short-trailer':data=data[:-5]
    elif variant=='concatenated':data+=gzip.compress(b'second',mtime=0)
    elif variant=='junk-tail':data+=b'authored nonmember tail of more than 26 bytes'
    elif variant in ('header-crc','bad-header-crc'):
        data[3]|=2
        crc=zlib.crc32(data[:10])&0xffff
        data=data[:10]+crc.to_bytes(2,'little')+data[10:]
        if variant=='bad-header-crc':data[10]^=1
    elif variant=='reserved-flags':data[3]|=0xe0
    elif variant=='optional-header':
        data[3]=0x1c
        data=data[:10]+b'\x03\x00abcname\x00comment\x00'+data[10:]
    elif variant!='normal':raise ValueError('unknown authored gzip variant')
    return bytes(data)
