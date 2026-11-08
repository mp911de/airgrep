"""Cell-center raster, deduplicated row runs versus hash-consed quadtree."""
import argparse
import json
import struct
import time
import numpy as np
import shapely
from source import ROOT, Source, YEAR

HEADER = struct.Struct('<4s15I64s')


def version(release, dataset):
    label = (release + '/' + dataset).encode('ascii')
    if len(label) >= 64:
        raise ValueError('release/dataset must fit 63 ASCII bytes')
    return label


def rasterize(source, step):
    width, height = round(360 / step), round(180 / step)
    grid = np.zeros((height, width), dtype='<u2')
    for zid, geom in enumerate(source.geoms, 1):
        for part in shapely.get_parts(geom):
            xmin, ymin, xmax, ymax = part.bounds
            x0 = max(0, int(np.floor((xmin + 180) / step)))
            x1 = min(width, int(np.ceil((xmax + 180) / step)))
            y0 = max(0, int(np.floor((ymin + 90) / step)))
            y1 = min(height, int(np.ceil((ymax + 90) / step)))
            if x1 <= x0 or y1 <= y0:
                continue
            shapely.prepare(part)
            xs = -180 + (np.arange(x0, x1) + .5) * step
            for row in range(y0, y1, 32):
                ys = -90 + (np.arange(row, min(row + 32, y1)) + .5) * step
                mask = shapely.intersects_xy(part, xs[None, :], ys[:, None])
                view = grid[row:row + len(ys), x0:x1]
                view[mask] = zid
    return grid


def row_runs(grid, zones):
    payload, index, seen = bytearray(), bytearray(), {}
    xbits, zbits = (grid.shape[1] - 1).bit_length(), (zones - 1).bit_length()
    bits = xbits + zbits
    for row in grid:
        starts = np.r_[0, np.flatnonzero(row[1:] != row[:-1]) + 1]
        record = bytearray(struct.pack('<H', len(starts)))
        packed = 0
        for i, x in enumerate(starts):
            packed |= (int(x) | (int(row[x]) << xbits)) << (i * bits)
        record += packed.to_bytes((len(starts) * bits + 7) // 8, 'little')
        key = bytes(record)
        if key not in seen:
            seen[key] = len(payload)
            payload += key
        index += seen[key].to_bytes(3, 'little')
    return bytes(index), bytes(payload), len(seen)


def quadtree(grid, zones):
    height, width = grid.shape
    size = 1 << (max(width, height) - 1).bit_length()
    nodes, seen = bytearray(), {}
    def visit(x, y, side):
        if x >= width or y >= height:
            return 0
        view = grid[y:min(y + side, height), x:min(x + side, width)]
        value = int(view[0, 0])
        if (value == 0 or (x + side <= width and y + side <= height)) and np.all(view == value):
            return value
        half = side // 2
        children = (visit(x, y, half), visit(x + half, y, half),
                    visit(x, y + half, half), visit(x + half, y + half, half))
        if children[0] < zones and len(set(children)) == 1:
            return children[0]
        if children not in seen:
            seen[children] = zones + len(nodes) // 12
            for ref in children:
                nodes.extend(ref.to_bytes(3, 'little'))
        return seen[children]
    root = visit(0, 0, size)
    return b'', bytes(nodes), root, size


def metadata(zones):
    strings, offsets = bytearray(), {}
    def put(s):
        if s not in offsets:
            offsets[s] = len(strings)
            strings.extend(s.encode('ascii') + b'\0')
        return offsets[s]
    index = bytearray()
    for z in zones:
        index.extend(struct.pack('<II', put(z['name']), put(z['posix'])))
    return bytes(index), bytes(strings)


def build(source, step, out):
    started = time.perf_counter()
    grid = rasterize(source, step)
    raster_seconds = time.perf_counter() - started
    grid.tofile(out / f'raster-{step:g}.bin')
    idx, runs, unique_rows = row_runs(grid, len(source.names))
    qi, nodes, root, side = quadtree(grid, len(source.names))
    assert len(runs) < 1 << 24 and len(nodes) // 12 + len(source.names) < 1 << 24
    zi, strings = metadata(source.zones)
    sizes = {'rows': HEADER.size + len(idx) + len(runs) + len(zi) + len(strings),
             'quadtree': HEADER.size + len(nodes) + len(zi) + len(strings)}
    for mode, index, payload, ref, span in [(1, idx, runs, 0, 0), (2, qi, nodes, root, side)]:
        offset_payload = HEADER.size + len(index)
        offset_zones = offset_payload + len(payload)
        offset_strings = offset_zones + len(zi)
        total = offset_strings + len(strings)
        header = HEADER.pack(b'TZL2', mode, round(step * 1000000), grid.shape[1], grid.shape[0],
                             len(source.names), HEADER.size, offset_payload, offset_zones, offset_strings,
                             total, ref, span, YEAR,
                             (grid.shape[1] - 1).bit_length() if mode == 1 else 0,
                             (len(source.names) - 1).bit_length() if mode == 1 else 0,
                             version(source.release, source.dataset))
        (out / f'tz-{step:g}-{"rows" if mode == 1 else "quadtree"}.bin').write_bytes(header + index + payload + zi + strings)
    selected = min(sizes, key=sizes.get)
    (out / f'tz-{step:g}.bin').write_bytes((out / f'tz-{step:g}-{selected}.bin').read_bytes())
    (out / 'tzdata.bin').write_bytes((out / f'tz-{step:g}.bin').read_bytes())
    elapsed = time.perf_counter() - started
    result = dict(cell_degrees=step, total_bytes=sizes[selected], encoding=selected,
                  header_bytes=HEADER.size, index_bytes=len(idx) if selected == 'rows' else 0,
                  payload_bytes=len(runs) if selected == 'rows' else len(nodes),
                  zone_index_bytes=len(zi), strings_bytes=len(strings), zones=len(source.names)-1,
                  unique_rows=unique_rows, candidates_bytes=sizes, build_seconds=round(elapsed, 3),
                  raster_seconds=round(raster_seconds, 3))
    print(json.dumps(result), flush=True)
    return result


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('--cell', type=float, nargs='+', default=[.01])
    p.add_argument('--release', default='2026f')
    p.add_argument('--dataset', default='timezones')
    args = p.parse_args()
    for step in args.cell:
        if step <= 0 or abs(step * 1000000 - round(step * 1000000)) > 1e-6 or abs(180 / step - round(180 / step)) > 1e-6 or 360 / step > 65535:
            p.error('cell must divide 180 degrees, use integer microdegrees, and fit 65535 columns')
    source = Source(args.release, args.dataset)
    out = ROOT / 'build'
    out.mkdir(exist_ok=True)
    results = [build(source, step, out) for step in args.cell]
    (out / 'sizes.json').write_text(json.dumps(results, indent=2) + '\n')
    (out / 'zones.json').write_text(json.dumps(source.zones, indent=2) + '\n')
