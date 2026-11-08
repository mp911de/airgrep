# GNSS position to time zone table

`make tzdata` produces `build/tzdata.bin`, an immutable, flash-ready TZL2 blob with local IANA names and per-zone POSIX rules. The source is the unmerged `timezones.geojson.zip` dataset from timezone-boundary-builder. The default raster cell is 0.01 degrees, approximately 1.11 km at the equator.

## Build and verify

Python 3 and a C99 compiler are required. Python dependencies, including `tzdata==2026.6`, are pinned in `requirements.txt` and installed into `venv/`.

```sh
make tzdata                         # RELEASE=2026f, DATASET=timezones, CELL=0.01
RELEASE=latest make                 # Resolve and print the latest GitHub release tag.
RELEASE=2026f CELL=0.02 make tzdata  # Another resolution.
make test                           # 50 points, exhaustive decoding, C/Python agreement.
make sanitize                       # ASan/UBSan synthetic, malformed and fixture checks.
make clean && make tzdata           # Rebuild generated artifacts.
venv/bin/python tools/lookup.py build/tzdata.bin 38.88 -7.01
make build/test_lookup
build/test_lookup build/tzdata.bin 38.88 -7.01
build/test_lookup build/tzdata.bin --info  # Rule year and release/dataset.
```

`RELEASE`, `DATASET` and `CELL` are Makefile defaults, overridable through the environment or make arguments. Only the unmerged `timezones` dataset is accepted. `latest` uses the GitHub releases API on each build. Repeat a resolved tag for reproducibility. Each download is checked against GitHub's asset SHA-256 digest when published. Missing digests are reported and pinning is skipped. Cached archives under `data/<tag>/` are checked again before use. GeoJSON is read directly from the verified archive. There is no source manifest.

Every invocation builds only its requested resolution and prints the flash blob's byte count and SHA-256. Cell sizes must be positive integer microdegrees, divide 180 degrees and fit at most 65,535 columns. Generated data stays under `build/`, downloaded data under `data/`. A 0.01-degree raw raster occupies 1,296,000,000 bytes as little-endian uint16 IDs. Building and exhaustive verification require several GB of RAM and disk space.

The builder retains packed row and shared-quadtree candidates for verification, selecting the smaller blob for `tzdata.bin`. `build/sizes.json` reports section and candidate sizes, `build/zones.json` records derived rules. These are generated reports, not source manifests. `make clean` removes build output and retains downloads.

## Measured sizes

For release `2026f`, with 419 zones plus `none`:

| Cell degrees | Selected bytes | Row index | Run payload | Quadtree bytes |
|---:|---:|---:|---:|---:|
| 0.01 | 1,750,417 | 54,000 | 1,684,604 | 3,298,349 |
| 0.02 | 830,129 | 27,000 | 791,316 | 1,827,173 |

Both selected blobs use packed rows. Each total also includes the 128-byte header, 3,360-byte zone directory and 8,325-byte string pool. Default blob SHA-256: `cba9e6147b224c1d7ea6feeacf53392978433a1cba083ee76482878b515e406b`. Repeated default builds produced the same hash. An aligned `0x1b0000`-byte partition fits the default blob.

## Raster

Full, unsimplified WGS84 polygons are sampled at cell centers. Rows run south to north from -90 degrees and columns west to east from -180 degrees. ID 0 is `none`, including uncovered oceans and polygon holes. Territorial waters retain their source zone. The 419 source names are sorted lexicographically into IDs 1 through 419. The highest ID wins overlaps or shared-boundary samples, in both the exact oracle and rasterizer. IDs are packed into 9 bits for this release.

Raster quantization can move a boundary by up to approximately 0.79 km at the equator with 0.01-degree cells. Source accuracy and GNSS error are additional uncertainties. Small regions without a cell center can disappear. No nearest-land assignment is performed.

## TZL2 format

All integers are unsigned little endian. Packed bits are least significant first. Sections have no alignment padding. TZL2 extends TZL1 with 64 source-identification bytes, so its header is 128 bytes. TZL1 blobs are rejected by this decoder.

| Offset | Bytes | Meaning |
|---:|---:|---|
| 0 | 4 | ASCII `TZL2` |
| 4 | 4 | Mode, 1 packed row runs, 2 shared quadtree |
| 8 | 4 | Cell side in integer microdegrees |
| 12 | 4 | Width, `360000000 / cell` |
| 16 | 4 | Height, `180000000 / cell` |
| 20 | 4 | Zone count including `none`, at most 65,535 |
| 24 | 4 | Absolute row index offset, always 128 |
| 28 | 4 | Absolute payload offset |
| 32 | 4 | Absolute zone directory offset |
| 36 | 4 | Absolute string pool offset |
| 40 | 4 | Exact blob length |
| 44 | 4 | Quadtree root reference, zero in mode 1 |
| 48 | 4 | Quadtree root square side, zero in mode 1 |
| 52 | 4 | Informational POSIX rule validity year, 2000 through 2200 inclusive |
| 56 | 4 | Column bit width X in mode 1, zero in mode 2 |
| 60 | 4 | Zone ID bit width Z in mode 1, zero in mode 2 |
| 64 | 64 | NUL-terminated ASCII `<resolved-tag>/<dataset>`, zero padded |

For the default build the version/comment is `2026f/timezones`. Identification is embedded in the flash blob itself. There is no embedded checksum. Verify deployment using the printed SHA-256.

### Packed row runs

The index contains `height` unsigned 24-bit payload-relative offsets. Identical rows share one record, ordered by first occurrence from south to north. Payload size must stay below 2^24 bytes.

A row starts with a uint16 run count N, then `ceil(N*(X+Z)/8)` packed bytes. X is `ceil(log2(width))`, Z is `ceil(log2(zone_count))`, each at most 16. A record value uses X low bits for the run's start column and Z high bits for its zone ID. No per-value padding is inserted. Unused high bits in the last byte are zero. Runs start at column zero, increase strictly and end at the next start or raster width.

The decoder reads X and Z from the header and binary-searches the row. Its packed reader may access up to four following bytes, within the validated blob because the zone directory follows the payload. Lookup uses constant RAM and O(log N) time.

### Shared quadtree

The row index is empty, so offsets 24 and 28 both equal 128. Each payload node holds four uint24 references in southwest, southeast, northwest, northeast order. A reference below `zone_count` is a uniform leaf. Otherwise its node index is `reference-zone_count`. References must point to earlier nodes, preventing cycles. Identical nodes share storage. Only four identical uniform leaves collapse.

The root covers the smallest power-of-two square containing the raster, padding outside cells with `none`. Each descent halves that square. Lookup uses constant RAM and at most 16 descents.

### Zone directory and API

Each zone directory entry is eight bytes: a uint32 name offset and a uint32 POSIX rule offset, both relative to the string pool. Strings are NUL-terminated ASCII, deduplicated by exact value. ID 0 has name `none` and an empty rule. There are no bytes after the string pool.

`tz_init` validates the complete immutable blob once, using its exact length, and empties the context on failure. Any rule year from 2000 through 2200 inclusive is accepted. The year is informational and need not match the decoder's build year. `tz_lookup` accepts signed integer microdegrees and returns a uint16 ID, or `TZ_INVALID` (65535) for invalid coordinates or an uninitialized context. Latitude must be within [-90,+90] and longitude within [-180,+180]. Positive 180 wraps to -180. Positive 90 uses the northernmost row. Exact internal cell edges belong to the northern/eastern cell.

`uint32_t tz_rule_year(const tz_table*)` exposes the header's rule year. `const char *tz_source(const tz_table*)` returns the release/dataset comment at header offset 64 directly from the blob. For an empty, failed-initialization or NULL context they return zero and NULL respectively. The source pointer shares the blob's lifetime. Existing API signatures and the TZL2 layout are unchanged.

`tz_name` and `tz_posix` return pointers into the blob, or NULL for invalid IDs. An empty POSIX rule requires explicit handling, never pass it to libc as a local rule. Retain the mapping for the pointers' lifetime. The context is one pointer, four bytes on ESP32-S3. Decoding uses no heap. Store the blob unchanged in a sufficiently large data partition, map it with `esp_partition_mmap`, and initialize using the exact blob length. Target execution, mapping and target libc behavior remain unverified.

## POSIX rules

Rules derive independently for each local IANA zone from pinned Python tzdata, without using the host timezone database or merging geographic zones. Hourly probes locate 2026 offset transitions, refined to the second. Rules encode this year's dates with `Mmonth.week.weekday/time`. Fractional offsets, southern seasons and unusual DST shifts are supported. One-transition years use a synthetic opposite transition at January 1 UTC.

Rule validity is limited to 2026 UTC timestamps, as recorded in the header. A later boundary release does not extend this validity year. Rebuild and revalidate the rule generator before using another year. POSIX recurrence strings cannot represent arbitrary IANA history or more than two transitions per year.

Pinned tzdata gives `Africa/Casablanca` and `Africa/El_Aaiun` three transitions in 2026. Their rules are empty, the build prints a warning, and the test summary lists them under `unavailable_posix`. Use their IANA names with a full timezone database for conversion. All other 417 zones have derived rules verified throughout 2026. ID 0 also has an empty rule because it has no assigned zone.

## Verification

The 50 coordinates in `test/points.csv` are retained. Expected zones are recomputed against the full unmerged polygons. `test/results.csv` reports expected/table names, match and distance to the nearest shared border with differing 2026 UTC offsets. Equal-clock borders are excluded from this distance, even though their local names remain distinct. The obsolete merge-status column is removed.

Python expands both encodings against every uint16 raster cell. The C driver independently checks every cell center against that raster. Additional C/Python comparisons include random coordinates, cell edges and adjacent microdegrees, poles and the antimeridian. Synthetic checks exercise repeated patterns and 1-, 6-, 8-, 9- and 16-bit IDs. Malformed headers and truncated blobs must be rejected. POSIX conversions are compared with pinned IANA zones at every hour of 2026, transition seconds and adjacent seconds, and the final second of the year.

The host metadata tests check both encodings with years 2000, 2026, 2027 and 2200, reject 0, 1999, 2201 and UINT32_MAX, and verify the returned source comment. The C driver also checks empty and NULL contexts and accessor results after failed initialization. These 16 metadata cases run under both `make test` and `make sanitize`.

The default fixture matches 44 of 50 points. All six misses are boundary quantization errors. Five are 0.202 to 0.278 km from differing-clock borders. The Gibraltar sample is 0.544 km from the Gibraltar/Spain name boundary, which has equal clocks in 2026 and is excluded from the CSV's differing-clock distance. The deliberately border-heavy fixture is not a population accuracy estimate.

Default verification passed 648,000,000 cells per encoding through both Python expansion and C lookup, 26,050 additional C/Python parity points, 10,000 independent polygon-center checks, 3,468 synthetic/width checks, 32 rejected malformed blobs, and 3,653,846 POSIX conversion checks for the 417 representable zones. Decoder and raster mismatches were zero. Both 0.02-degree encodings also passed exhaustive comparison over 162,000,000 cells each and 26,050 parity points. Host compilation uses C99 with `-Wall -Wextra -Werror -pedantic`. Address/undefined-behavior sanitizers passed 3,468 synthetic/width checks, 32 malformed blobs and 100 fixture lookups after a clean default rebuild.

## Licenses

Boundary data and derived lookup databases are [ODbL 1.0](https://opendatacommons.org/licenses/odbl/1-0/). Attribution: timezone-boundary-builder and © OpenStreetMap contributors. Preserve this attribution and the ODbL license with distributed tables. The [upstream builder](https://github.com/evansiroky/timezone-boundary-builder#licenses) code has a separate MIT license.

IANA tzdb data is public domain. Python tzdata packaging uses Apache-2.0. NumPy uses BSD-3-Clause, Shapely BSD-3-Clause, and pyproj MIT. These dependencies are build tools and are not embedded in the blob. The existing random coordinates were selected using Natural Earth's public-domain land mask. That mask is no longer downloaded or required by this pipeline. This directory does not declare a separate license for its own decoder and build scripts.
