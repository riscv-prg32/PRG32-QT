#!/usr/bin/env python3
"""Certify portable PRG32 Store variants using the headless runtime."""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import re
import ssl
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request

DEFAULT_STORE = "https://store.prg32.uniparthenope.it/"
PORTABLE_ARCHITECTURES = ("qt", "qemu", "ios")
NONPORTABLE_ERRORS = (
    "PRG32-QT accepts portable ABI-table PRG32 cartridges only",
    "cartridge requires unavailable features",
)
MEDIA_PATTERN = re.compile(
    r"MEDIA graphics_non_black=(\d+) unique_frame_hashes=(\d+) "
    r"audio_declared=(\d+) audio_events=(\d+) pcm_samples=(\d+)"
)


# HTTP statuses that report a temporarily overloaded or rate-limited Store rather than a missing package.
TRANSIENT_HTTP_STATUSES = (429, 500, 502, 503, 504)
FETCH_ATTEMPTS = 5
FETCH_BACKOFF_SECONDS = 2.0
# Pause between package downloads so a whole-Store run does not trip the server's rate limiter.
DOWNLOAD_INTERVAL_SECONDS = 0.5
# CA bundles tried when the interpreter has no usable default trust store (python.org macOS builds).
FALLBACK_CA_BUNDLES = ("/etc/ssl/cert.pem", "/etc/ssl/certs/ca-certificates.crt")


def tls_context() -> ssl.SSLContext:
    """Return a verifying TLS context, falling back to a system CA bundle when the default store is empty."""
    context = ssl.create_default_context()
    if context.cert_store_stats().get("x509_ca", 0):
        return context
    for bundle in FALLBACK_CA_BUNDLES:
        if pathlib.Path(bundle).is_file():
            return ssl.create_default_context(cafile=bundle)
    return context


TLS_CONTEXT = tls_context()


def fetch(url: str) -> bytes:
    """Read a Store endpoint with a bounded timeout, retrying transient server and network failures."""
    request = urllib.request.Request(url, headers={"User-Agent": "PRG32-QT-store-certification/0.4"})
    for attempt in range(FETCH_ATTEMPTS):
        try:
            with urllib.request.urlopen(request, timeout=20, context=TLS_CONTEXT) as response:
                return response.read()
        except urllib.error.HTTPError as error:
            if error.code not in TRANSIENT_HTTP_STATUSES or attempt + 1 == FETCH_ATTEMPTS:
                raise
            retry_after = error.headers.get("Retry-After", "")
            delay = float(retry_after) if retry_after.isdigit() else FETCH_BACKOFF_SECONDS * 2**attempt
        except urllib.error.URLError as error:
            # Certificate failures are configuration errors, not transient outages.
            if isinstance(error.reason, ssl.SSLError) or attempt + 1 == FETCH_ATTEMPTS:
                raise
            delay = FETCH_BACKOFF_SECONDS * 2**attempt
        except TimeoutError:
            if attempt + 1 == FETCH_ATTEMPTS:
                raise
            delay = FETCH_BACKOFF_SECONDS * 2**attempt
        time.sleep(delay)
    raise RuntimeError("unreachable: fetch retry loop exited without a result")


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


def cached_fetch(url: str, cache_directory: pathlib.Path | None) -> bytes:
    """Download a package once per URL so accurate and Unlimited runs certify identical bytes."""
    if cache_directory is None:
        return fetch(url)
    cache_key = hashlib.sha256(url.encode("utf-8")).hexdigest()
    cache_path = cache_directory / f"{cache_key}.prg32"
    if cache_path.is_file():
        return cache_path.read_bytes()
    package = fetch(url)
    time.sleep(DOWNLOAD_INTERVAL_SECONDS)
    cache_directory.mkdir(parents=True, exist_ok=True)
    cache_path.write_bytes(package)
    return package


def certify(
    store: str,
    runner: str,
    frames: int,
    unlimited: bool = False,
    cache_directory: pathlib.Path | None = None,
) -> int:
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
                package = cached_fetch(url, cache_directory)
                if not package.startswith(b"PRG2"):
                    raise ValueError("download is not a PRG2 package")
                package_path = pathlib.Path(temporary_directory) / f"cartridge-{index}.prg32"
                package_path.write_bytes(package)
                command = [runner, str(package_path), str(frames), "--verify-media"]
                if unlimited:
                    command.append("--unlimited")
                result = subprocess.run(
                    command,
                    capture_output=True,
                    text=True,
                    timeout=120,
                    check=False,
                )
                if result.returncode:
                    detail = (result.stderr or result.stdout).strip()
                    nonportable_reason = next((reason for reason in NONPORTABLE_ERRORS if reason in detail), None)
                    if nonportable_reason:
                        print(f"SKIP {label}: {detail}")
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
    parser.add_argument("--unlimited", action="store_true", help="certify the runner's Unlimited mode")
    parser.add_argument("--cache-dir", type=pathlib.Path, help="reuse downloaded packages across runs")
    arguments = parser.parse_args()
    try:
        return certify(arguments.store, arguments.runner, arguments.frames, arguments.unlimited, arguments.cache_dir)
    except (OSError, ValueError, RuntimeError) as error:
        print(f"FAIL: Store certification could not start: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
