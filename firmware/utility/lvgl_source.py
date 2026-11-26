"""Locate the firmware's locked LVGL package for the macOS build."""

import argparse
import sys
from contextlib import redirect_stdout
from fcntl import LOCK_EX, flock
from pathlib import Path
from tempfile import TemporaryDirectory

from idf_component_tools.hash_tools.errors import ValidatingHashError
from idf_component_tools.hash_tools.validate import validate_hash_eq_hashdir
from idf_component_tools.lock import LockManager


def matches_package(path, component):
    if not (path / "idf_component.yml").is_file():
        return False
    try:
        validate_hash_eq_hashdir(path, component.component_hash)
        return True
    except ValidatingHashError:
        return False


def lvgl_source(lockfile, version, offline):
    component = LockManager(lockfile).load().solved_components.get("lvgl/lvgl")
    if component is None or str(component.version) != version:
        sys.exit(
            f"Lockfile does not contain LVGL {version}. Run cmake --preset s3."
        )

    cache = Path(component.source.component_cache_path(component))
    for path in (cache, lockfile.parent / "managed_components/lvgl__lvgl"):
        if matches_package(path, component):
            return path
    if offline:
        sys.exit(f"No verified local copy of LVGL {version} for this offline build.")

    # Concurrent Utility builds share the Component Manager's cache.
    cache.parent.mkdir(parents=True, exist_ok=True)
    with cache.with_name(cache.name + ".lock").open("a") as lock:
        flock(lock, LOCK_EX)
        if not matches_package(cache, component):
            with TemporaryDirectory() as directory:
                component.source.download(component, str(Path(directory) / "lvgl"))
            validate_hash_eq_hashdir(cache, component.component_hash)
    return cache


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("lockfile", type=Path)
    parser.add_argument("version")
    parser.add_argument("--offline", action="store_true")
    args = parser.parse_args()
    with redirect_stdout(sys.stderr):
        source = lvgl_source(args.lockfile, args.version, args.offline)
    print(source)
