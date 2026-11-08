"""Reference reader for TZL2. Coordinates enter as integer microdegrees."""
import argparse
import json
import struct
from pathlib import Path
import numpy as np


class Table:
    def __init__(self, path):
        self.data = Path(path).read_bytes()
        h = struct.unpack_from('<4s15I64s', self.data)
        if h[0] != b'TZL2' or h[10] != len(self.data):
            raise ValueError('invalid TZL2 header')
        (self.mode, self.step, self.width, self.height, self.zones, self.index,
         self.payload, self.zone_index, self.strings, _, self.root, self.side,
         self.year, self.xbits, self.zbits) = h[1:16]
        self.version = h[16].split(b'\0', 1)[0].decode('ascii')

    def u24(self, offset):
        return int.from_bytes(self.data[offset:offset + 3], 'little')

    def record(self, row, i):
        bits = self.xbits + self.zbits
        bit = i * bits
        start = row + 2 + bit // 8
        value = int.from_bytes(self.data[start:start + 5], 'little') >> (bit % 8)
        value &= (1 << bits) - 1
        return value & ((1 << self.xbits) - 1), value >> self.xbits

    def cell(self, x, y):
        if self.mode == 1:
            row = self.payload + self.u24(self.index + 3 * y)
            count = struct.unpack_from('<H', self.data, row)[0]
            lo, hi = 0, count
            while lo + 1 < hi:
                mid = (lo + hi) // 2
                if self.record(row, mid)[0] <= x:
                    lo = mid
                else:
                    hi = mid
            return self.record(row, lo)[1]
        ref, span = self.root, self.side
        while ref >= self.zones:
            span //= 2
            child = (int(y >= span) << 1) | int(x >= span)
            x, y = x % span, y % span
            ref = self.u24(self.payload + 12 * (ref - self.zones) + 3 * child)
        return ref

    def lookup(self, lat, lon):
        if not (-90000000 <= lat <= 90000000 and -180000000 <= lon <= 180000000):
            raise ValueError('coordinates out of range')
        x = 0 if lon == 180000000 else (lon + 180000000) // self.step
        y = min(self.height - 1, (lat + 90000000) // self.step)
        return self.cell(x, y)

    def zone(self, zid):
        def string(offset):
            start = self.strings + offset
            return self.data[start:self.data.index(b'\0', start)].decode('ascii')
        name, rule = struct.unpack_from('<II', self.data, self.zone_index + 8 * zid)
        return string(name), string(rule)

    def expand(self):
        grid = np.empty((self.height, self.width), dtype='<u2')
        if self.mode == 1:
            for y in range(self.height):
                row = self.payload + self.u24(self.index + 3 * y)
                count = struct.unpack_from('<H', self.data, row)[0]
                records = [self.record(row, i) for i in range(count)]
                for i, (x, zid) in enumerate(records):
                    end = records[i + 1][0] if i + 1 < count else self.width
                    grid[y, x:end] = zid
        else:
            def visit(ref, x, y, span):
                if x >= self.width or y >= self.height:
                    return
                if ref < self.zones:
                    grid[y:min(y + span, self.height), x:min(x + span, self.width)] = ref
                    return
                half = span // 2
                for child in range(4):
                    visit(self.u24(self.payload + 12 * (ref - self.zones) + child * 3),
                          x + (child % 2) * half, y + (child // 2) * half, half)
            visit(self.root, 0, 0, self.side)
        return grid


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('table')
    p.add_argument('lat', type=float)
    p.add_argument('lon', type=float)
    a = p.parse_args()
    table = Table(a.table)
    zid = table.lookup(round(a.lat * 1000000), round(a.lon * 1000000))
    print(json.dumps(dict(id=zid, zone=table.zone(zid)[0], posix=table.zone(zid)[1])))
