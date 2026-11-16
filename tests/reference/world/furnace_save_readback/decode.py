"""Independent bounded typed NBT decoder; no game implementation."""
import struct
import zlib
BOUND=1048576

def require(value,message):
    if not value: raise AssertionError(message)

class Nbt:
    def __init__(self, data):
        self.data, self.at, self.tags = data, 0, 0

    def take(self, n):
        require(0 <= n <= len(self.data) - self.at, 'bounded NBT extent')
        value = self.data[self.at:self.at + n]
        self.at += n
        return value

    def number(self, fmt):
        return struct.unpack('>' + fmt, self.take(struct.calcsize(fmt)))[0]

    def string(self):
        raw, units, at = self.take(self.number('H')), [], 0
        while at < len(raw):
            lead = raw[at]
            at += 1
            if 1 <= lead <= 127:
                units.append(lead)
            elif lead & 224 == 192:
                require(at < len(raw) and raw[at] & 192 == 128, 'MUTF8 continuation')
                value = ((lead & 31) << 6) | (raw[at] & 63)
                at += 1
                require(value == 0 or value >= 128, 'canonical MUTF8 pair')
                units.append(value)
            else:
                require(lead & 240 == 224 and at + 1 < len(raw) and raw[at] & 192 == raw[at + 1] & 192 == 128,
                        'MUTF8 triple')
                value = ((lead & 15) << 12) | ((raw[at] & 63) << 6) | (raw[at + 1] & 63)
                at += 2
                require(value >= 2048, 'canonical MUTF8 triple')
                units.append(value)
        return ''.join(map(chr, units))

    def payload(self, kind, depth):
        self.tags += 1
        require(depth <= 64 and self.tags <= 10000, 'bounded NBT graph')
        if kind in range(1, 7):
            return self.number({1: 'b', 2: 'h', 3: 'i', 4: 'q', 5: 'I', 6: 'Q'}[kind])
        if kind == 7:
            n = self.number('i')
            require(0 <= n <= BOUND, 'bounded ByteArray')
            return self.take(n)
        if kind == 8:
            return self.string()
        if kind == 9:
            child, n = self.number('B'), self.number('i')
            require(0 <= child <= 10 and 0 <= n <= 10000, 'bounded typed list')
            return [child, [self.payload(child, depth + 1) for _ in range(n)]]
        require(kind == 10, 'actual Compound type')
        result = {}
        while True:
            child = self.number('B')
            if child == 0:
                return result
            key = self.string()
            require(key not in result, 'unique Compound names')
            result[key] = [child, self.payload(child, depth + 1)]

    def root(self):
        require(self.number('B') == 10 and self.string() == '', 'single unnamed Compound root')
        result = self.payload(10, 1)
        require(self.at == len(self.data), 'complete NBT/no trailing bytes')
        return result


def inflate(data, window):
    decoder = zlib.decompressobj(window)
    raw = decoder.decompress(data, BOUND + 1)
    require(len(raw) <= BOUND and decoder.eof and not decoder.unused_data and not decoder.unconsumed_tail,
            'complete bounded compressed member/checksum/trailer')
    return Nbt(raw).root()
