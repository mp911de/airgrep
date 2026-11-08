"""Release boundary input, exact oracle, and 2026 TZ rules."""
import calendar
import datetime as dt
import hashlib
import importlib.resources as resources
import json
from pathlib import Path
import urllib.request
import zipfile
from zoneinfo import ZoneInfo
import shapely
from shapely.geometry import shape, Point

ROOT = Path(__file__).resolve().parents[1]
YEAR = 2026


def fetch(tag, dataset):
    if dataset != 'timezones':
        raise ValueError('dataset must be timezones, the unmerged land zones')
    endpoint = 'latest' if tag == 'latest' else 'tags/' + tag
    request = urllib.request.Request(
        'https://api.github.com/repos/evansiroky/timezone-boundary-builder/releases/' + endpoint,
        headers={'Accept': 'application/vnd.github+json', 'User-Agent': 'tzdb-builder'})
    with urllib.request.urlopen(request) as response:
        release = json.load(response)
    resolved = release['tag_name']
    if tag != 'latest' and resolved != tag:
        raise ValueError('release tag mismatch')
    print('Resolved release:', resolved, flush=True)
    folder = ROOT / 'data' / resolved
    folder.mkdir(parents=True, exist_ok=True)
    asset = next(a for a in release['assets'] if a['name'] == dataset + '.geojson.zip')
    archive = folder / asset['name']
    if not archive.exists():
        urllib.request.urlretrieve(asset['browser_download_url'], archive)
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    published = asset.get('digest')
    if published and published.startswith('sha256:'):
        if published != 'sha256:' + digest:
            raise ValueError('archive SHA-256 differs from GitHub digest: ' + str(archive))
        print('Verified GitHub SHA-256:', digest, flush=True)
    else:
        print('GitHub SHA-256 unavailable, skipping pinning', flush=True)
    with zipfile.ZipFile(archive) as z:
        data = json.loads(z.read('combined.json'))
    return resolved, digest, data


class Source:
    def __init__(self, release='2026f', dataset='timezones'):
        self.release, self.digest, data = fetch(release, dataset)
        self.dataset = dataset
        features = sorted(data['features'], key=lambda f: f['properties']['tzid'])
        self.names = ['none'] + [f['properties']['tzid'] for f in features]
        assert len(set(self.names)) == len(self.names) and len(self.names) <= 65535
        self.geoms = [shape(f['geometry']) for f in features]
        assert all(g.is_valid for g in self.geoms)
        self.tree = shapely.STRtree(self.geoms)
        self.zones = [{'name': 'none', 'posix': ''}] + [rule(n) for n in self.names[1:]]

    def exact(self, lat, lon):
        p = Point(lon, lat)
        ids = self.tree.query(p, predicate='intersects')
        return int(max(ids)) + 1 if len(ids) else 0


def tz(name):
    path = resources.files('tzdata.zoneinfo').joinpath(*name.split('/'))
    with path.open('rb') as f:
        return ZoneInfo.from_file(f, key=name)


def offset_text(seconds):
    sign = '-' if seconds < 0 else ''
    seconds = abs(seconds)
    h, remainder = divmod(seconds, 3600)
    m, s = divmod(remainder, 60)
    return sign + str(h) + (f':{m:02}' if remainder else '') + (f':{s:02}' if s else '')


def rule(name):
    zone = tz(name)
    utc = dt.timezone.utc
    start, end = [dt.datetime(y, 1, 1, tzinfo=utc) for y in (YEAR, YEAR + 1)]
    def state(t):
        local = t.astimezone(zone)
        return int(local.utcoffset().total_seconds()), local.tzname()
    transitions = []
    previous = start
    cursor = start + dt.timedelta(hours=1)
    while cursor <= end:
        if state(previous)[0] != state(cursor)[0]:
            lo, hi = int(previous.timestamp()), int(cursor.timestamp())
            old = state(previous)
            while hi - lo > 1:
                mid = (lo + hi) // 2
                if state(dt.datetime.fromtimestamp(mid, utc))[0] == old[0]:
                    lo = mid
                else:
                    hi = mid
            instant = dt.datetime.fromtimestamp(hi, utc)
            transitions.append((instant, old, state(instant)))
        previous, cursor = cursor, cursor + dt.timedelta(hours=1)
    offsets = sorted({state(start)[0]} | {v[2][0] for v in transitions})
    if len(offsets) > 2 or len(transitions) > 2:
        print(f'Warning: {name} cannot fit one {YEAR} POSIX rule, storing an empty rule', flush=True)
        return {'name': name, 'posix': '',
                'transitions_utc': [t.isoformat() for t, _, _ in transitions]}
    standard = offsets[0]
    def abbr(offset):
        candidates = [state(start)] + [v[2] for v in transitions]
        a = next(a for o, a in candidates if o == offset)
        return a if a.isalpha() and len(a) >= 3 else '<' + a + '>'
    text = abbr(standard) + offset_text(-standard)
    if transitions:
        daylight = offsets[1]
        text += abbr(daylight)
        if daylight != standard + 3600:
            text += offset_text(-daylight)
        def date_rule(t, before):
            local = (t + dt.timedelta(seconds=before)).replace(tzinfo=None)
            week = (local.day - 1) // 7 + 1
            if local.day + 7 > calendar.monthrange(YEAR, local.month)[1]:
                week = 5
            weekday = (local.weekday() + 1) % 7
            seconds = local.hour * 3600 + local.minute * 60 + local.second
            return f'M{local.month}.{week}.{weekday}/' + offset_text(seconds)
        forward = next((v for v in transitions if v[2][0] > v[1][0]), None)
        backward = next((v for v in transitions if v[2][0] < v[1][0]), None)
        # A synthetic transition at Jan 1 UTC bounds a one-transition year.
        def year_edge(before):
            return ('J365/' + offset_text(86400 + before)) if before < 0 else ('J1/' + offset_text(before))
        begin = date_rule(forward[0], forward[1][0]) if forward else year_edge(standard)
        finish = date_rule(backward[0], backward[1][0]) if backward else year_edge(daylight)
        text += ',' + begin + ',' + finish
    return {'name': name, 'posix': text, 'transitions_utc': [t.isoformat() for t, _, _ in transitions]}


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--release', default='2026f')
    parser.add_argument('--dataset', default='timezones')
    args = parser.parse_args()
    fetch(args.release, args.dataset)
