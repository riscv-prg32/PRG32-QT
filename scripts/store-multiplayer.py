#!/usr/bin/env python3
"""Certify every multiplayer cartridge in the PRG32 Cartridge Store on several PRG32-QT consoles at once.

Each Store entry whose metadata declares `multiplayer: true`, or that carries the `multiplayer` tag, is
downloaded (portable variant, the catalog's `latest_version`) and run by `prg32qt_multiplayer_check` on
several runtimes in one process, each with its own prg32-multiplayer-1.0 connection. Scripted taps walk
the cartridge's menus to its online lobby. A cartridge passes when every console is admitted with its
own player ID (ABI #139) and sees every other console as a peer (ABI #39/#40).

    python3 scripts/store-multiplayer.py --checker ./build-macos/prg32qt_multiplayer_check \
        --server discover --cache-dir store-packages

`--server discover` sets no address, so the consoles find a PRG32 Multiplayer Server on the local
network by LAN discovery, as the firmware does; a ws:// or wss:// URL uses that server. A multiplayer
cartridge without a lobby script below fails: add its menu path rather than skipping it.
"""

from __future__ import annotations

import argparse
import importlib.util
import json
import pathlib
import subprocess
import sys
import tempfile

# The Store client (catalog, download URL, TLS fallback, cache) is shared with the whole-Store run.
_SMOKE_PATH = pathlib.Path(__file__).with_name("store-smoke.py")
_SMOKE_SPEC = importlib.util.spec_from_file_location("store_smoke", _SMOKE_PATH)
store_smoke = importlib.util.module_from_spec(_SMOKE_SPEC)
_SMOKE_SPEC.loader.exec_module(store_smoke)

# Menu path to each game's online lobby, as `--keys` taps (KEY@frame, 30 frames per second), and the
# seconds to run. Frame numbers leave room for each screen's transition; a tap is held for 4 frames.
LOBBY_SCRIPTS = {
    # Title menu: four rows down to PLAY ONLINE; the lobby counts the consoles.
    "org.riscv-prg32.azzurra-pro-tennis": ("DOWN@100,DOWN@120,DOWN@140,DOWN@160,A@180", 14),
    # UP selects the network battle on the title; A joins the lobby.
    "org.riscv-prg32.drumfight-napoli97": ("UP@60,A@80", 12),
    # Title, NETWORK MULTIPLAYER, syndicate, Team HQ down to RACE WEEKEND, course brief: then the lobby.
    "org.riscv-prg32.fairwind-napoli97": (
        "A@100,DOWN@170,A@210,A@260,DOWN@300,DOWN@320,DOWN@340,DOWN@360,DOWN@380,A@410,A@450",
        20,
    ),
    # Joins at boot; the shared world publishes snapshots once a game is under way.
    "org.riscv-prg32.galleria2007": ("A@100,A@160,A@220,A@280", 15),
    "org.riscv-prg32.galleria2007.en": ("A@100,A@160,A@220,A@280", 15),
    # Title, GAME MODE: NETWORK 2-4 PLAYERS, the Fiat 126: then the lobby.
    "org.riscv-prg32.outbun-napoli97": ("A@120,DOWN@150,A@180,RIGHT@210,A@240", 14),
    # Both join at boot.
    "org.riscv-prg32.sensational-soccer": ("", 12),
    "org.riscv-prg32.skipass-madness-after-christmas": ("", 12),
}


def is_multiplayer(item: dict) -> bool:
    """True when the Store entry declares multiplayer in its metadata or tags."""
    metadata = item.get("metadata") if isinstance(item.get("metadata"), dict) else {}
    if metadata.get("multiplayer") is True:
        return True
    tags = list(item.get("tags") or []) + list(metadata.get("tags") or [])
    return "multiplayer" in tags


def certify(store: str, checker: str, server: str, consoles: int, cache: pathlib.Path | None) -> int:
    """Print an ID/version/result snapshot and fail on any multiplayer cartridge that does not pass."""
    items = [item for item in store_smoke.catalog(store) if isinstance(item, dict) and is_multiplayer(item)]
    passed = failed = 0
    with tempfile.TemporaryDirectory() as temporary:
        for index, item in enumerate(items):
            identifier = str(item.get("id", "?"))
            version = str(item.get("latest_version") or item.get("version") or "unknown")
            label = f"{identifier} {version}"
            if identifier not in LOBBY_SCRIPTS:
                print(f"FAIL {label}: no lobby script in scripts/store-multiplayer.py")
                failed += 1
                continue
            keys, seconds = LOBBY_SCRIPTS[identifier]
            url = store_smoke.download_url(store, dict(item, selected_version=version))
            if url is None:
                print(f"FAIL {label}: no portable variant")
                failed += 1
                continue
            package = store_smoke.cached_fetch(url, cache)
            path = pathlib.Path(temporary) / f"cartridge-{index}.prg32"
            path.write_bytes(package)
            command = [checker, str(path), server, str(consoles), str(seconds)]
            if keys:
                command += ["--keys", keys]
            result = subprocess.run(command, capture_output=True, text=True, timeout=seconds + 60)
            consoles_lines = [line for line in result.stdout.splitlines() if line.startswith("CONSOLE")]
            verdict = "PASS" if result.returncode == 0 else "FAIL"
            print(f"{verdict} {label}: " + "; ".join(consoles_lines))
            if result.returncode == 0:
                passed += 1
            else:
                failed += 1
    print(f"multiplayer_catalog={len(items)} passed={passed} failed={failed} server={server} consoles={consoles}")
    return 0 if passed > 0 and failed == 0 else 1


def main() -> int:
    """Parse options and run the certification."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--store", default=store_smoke.DEFAULT_STORE)
    parser.add_argument("--checker", default="./build-macos/prg32qt_multiplayer_check")
    parser.add_argument("--server", default="discover", help="discover, or a ws:// or wss:// server URL")
    parser.add_argument("--consoles", type=int, default=2)
    parser.add_argument("--cache-dir", type=pathlib.Path, help="reuse downloaded packages across runs")
    arguments = parser.parse_args()
    try:
        return certify(arguments.store, arguments.checker, arguments.server, arguments.consoles, arguments.cache_dir)
    except (OSError, ValueError, RuntimeError, json.JSONDecodeError) as error:
        print(f"FAIL: Store multiplayer certification could not start: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
