#!/usr/bin/env python3
"""Certify portable PRG32 Store variants using the headless runtime."""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import subprocess
import sys
import tempfile
import urllib.parse
import urllib.request

DEFAULT_STORE = "http://193.205.230.7:5080"
PORTABLE_ARCHITECTURES = ("qt", "qemu", "ios")
NONPORTABLE_ERROR = "PRG32-QT accepts portable ABI-table PRG32 cartridges only"
MEDIA_PATTERN = re.compile(
    r"MEDIA graphics_non_black=(\d+) unique_frame_hashes=(\d+) "
    r"audio_declared=(\d+) audio_events=(\d+) pcm_samples=(\d+)"
)


def fetch(url: str) -> bytes:
    """Read a Store endpoint with a bounded timeout."""
    request = urllib.request.Request(url, headers={"User-Agent": "PRG32-QT-store-certification/0.3"})
    with urllib.request.urlopen(request, timeout=20) as response:
        return response.read()


def store_url(base: str, path: str) -> str:
    """Resolve a Store-relative URL without rewriting an absolute URL."""
    return urllib.parse.urljoin(base.rstrip("/") + "/", path)


def catalog_items(document: object) -> list | None:
    """Accept both list catalogs and the public API envelope."""
    if isinstance(document, list):
        return document
    if isinstance(document, dict):
        for key in ("games", "cartridges", "items", "results"):
            if isinstance(document.get(key), list):
                return document[key]
    return None


def catalog(store: str) -> list:
    """Read the current catalog through the game API or discovery document."""
    try:
        items = catalog_items(json.loads(fetch(store_url(store, "api/games"))))
        if items is not None:
            return items
    except (OSError, ValueError):
        pass
    discovery = json.loads(fetch(store_url(store, ".well-known/prg32-store.json")))
    if discovery.get("abi") != "prg32-store-discovery-1.0":
        raise RuntimeError("unsupported Store discovery ABI")
    endpoints = [
        discovery[key]
        for key in ("catalog", "catalog_url", "cartridges", "cartridges_url", "api")
        if isinstance(discovery.get(key), str)
    ]
    endpoints += ["api/games", "api/cartridges", "cartridges.json", "catalog.json"]
    for endpoint in endpoints:
        try:
            items = catalog_items(json.loads(fetch(store_url(store, endpoint))))
            if items is not None:
                return items
        except (OSError, ValueError):
            pass
    raise RuntimeError("no supported catalog endpoint")


def download_url(store: str, item: dict) -> str | None:
    """Resolve string- or object-valued portable variants in catalog order."""
    direct = next(
        (item[key] for key in ("download_url", "url", "package_url", "cartridge_url") if isinstance(item.get(key), str)),
        None,
    )
    variants = item.get("variants")
    if isinstance(variants, dict):
        for architecture in PORTABLE_ARCHITECTURES:
            variant = variants.get(architecture)
            if isinstance(variant, str):
                direct = variant
                break
            if isinstance(variant, dict):
                candidate = next(
                    (variant[key] for key in ("download_url", "url") if isinstance(variant.get(key), str)),
                    None,
                )
                if candidate:
                    direct = candidate
                    break
    if direct:
        return store_url(store, direct)

    identifier = item.get("id")
    offered = item.get("architectures")
    if not isinstance(identifier, str) or not identifier or not isinstance(offered, list):
        return None
    architecture = next((name for name in PORTABLE_ARCHITECTURES if name in offered), None)
    if architecture is None:
        return None
    query = {"architecture": architecture}
    version = item.get("selected_version") or item.get("version") or item.get("latest_version")
    if version:
        query["version"] = str(version)
    endpoint = "api/games/" + urllib.parse.quote(identifier, safe="") + "/download"
    return store_url(store, endpoint) + "?" + urllib.parse.urlencode(query)


def certify(store: str, runner: str, frames: int) -> int:
    """Print an ID/version/result snapshot and fail for portable runtime errors."""
    items = catalog(store)
    if not items:
        print("FAIL: Store catalog is empty", file=sys.stderr)
        return 1
    passed = skipped = failed = 0
    with tempfile.TemporaryDirectory() as temporary_directory:
        for index, item in enumerate(items):
            if not isinstance(item, dict):
                print(f"FAIL item-{index}: invalid catalog entry")
                failed += 1
                continue
            identifier = str(item.get("id") or item.get("name") or f"item-{index}")
            version = str(item.get("selected_version") or item.get("version") or item.get("latest_version") or "unknown")
            label = f"{identifier} {version}"
            url = download_url(store, item)
            if not url:
                print(f"SKIP {label}: no portable variant advertised")
                skipped += 1
                continue
            try:
                package = fetch(url)
                if not package.startswith(b"PRG2"):
                    raise ValueError("download is not a PRG2 package")
                package_path = pathlib.Path(temporary_directory) / f"cartridge-{index}.prg32"
                package_path.write_bytes(package)
                result = subprocess.run(
                    [runner, str(package_path), str(frames), "--verify-media"],
                    capture_output=True,
                    text=True,
                    timeout=120,
                    check=False,
                )
                if result.returncode:
                    detail = (result.stderr or result.stdout).strip()
                    if NONPORTABLE_ERROR in detail:
                        print(f"SKIP {label}: {NONPORTABLE_ERROR}")
                        skipped += 1
                    else:
                        print(f"FAIL {label}: {detail}")
                        failed += 1
                else:
                    media = MEDIA_PATTERN.search(result.stdout)
                    if not media:
                        print(f"FAIL {label}: runner did not emit media evidence")
                        failed += 1
                        continue
                    non_black, hashes, audio_declared, audio_events, pcm_samples = map(int, media.groups())
                    audio_status = "active" if audio_events else ("declared-idle" if audio_declared else "not-declared")
                    print(
                        f"PASS {label}: {frames} frames; graphics={non_black}px/{hashes} hashes; "
                        f"audio={audio_status} events={audio_events} pcm_samples={pcm_samples}"
                    )
                    passed += 1
            except (OSError, ValueError, subprocess.TimeoutExpired) as error:
                print(f"FAIL {label}: {error}")
                failed += 1
    print(f"catalog={len(items)} passed={passed} skipped_nonportable={skipped} failed={failed}")
    return 0 if passed > 0 and failed == 0 else 1


def main() -> int:
    """Parse options and report discovery failures as a failed certification."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--store", default=DEFAULT_STORE)
    parser.add_argument("--runner", default="./build-core/prg32qt-headless")
    parser.add_argument("--frames", type=int, default=300)
    arguments = parser.parse_args()
    try:
        return certify(arguments.store, arguments.runner, arguments.frames)
    except (OSError, ValueError, RuntimeError) as error:
        print(f"FAIL: Store certification could not start: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
