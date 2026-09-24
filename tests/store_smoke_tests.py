"""Regression checks for Store catalog resolution and certification status."""

import importlib.util
import pathlib
import subprocess
import unittest
from unittest import mock


SCRIPT = pathlib.Path(__file__).resolve().parents[1] / "scripts/store-smoke.py"
SPECIFICATION = importlib.util.spec_from_file_location("store_smoke", SCRIPT)
store_smoke = importlib.util.module_from_spec(SPECIFICATION)
SPECIFICATION.loader.exec_module(store_smoke)


class StoreSmokeTests(unittest.TestCase):
    """Ensure catalog schema changes cannot silently produce a false-green job."""

    def test_object_variant_uses_versioned_download_endpoint(self):
        item = {
            "id": "org.example.game",
            "selected_version": "1.2.3",
            "architectures": ["esp32c6", "qemu"],
            "variants": {"qemu": {"filename": "private/qemu.prg32"}},
        }
        self.assertEqual(
            store_smoke.download_url("https://store.example", item),
            "https://store.example/api/games/org.example.game/download?architecture=qemu&version=1.2.3",
        )

    def test_empty_catalog_fails(self):
        with mock.patch.object(store_smoke, "catalog", return_value=[]):
            self.assertEqual(store_smoke.certify("https://store.example", "runner", 300), 1)

    def test_nonportable_package_is_reported_as_skipped(self):
        item = {"id": "org.example.legacy", "version": "1.0.0", "architectures": ["qemu"]}
        rejection = subprocess.CompletedProcess([], 1, "", store_smoke.NONPORTABLE_ERRORS[0])
        with mock.patch.object(store_smoke, "catalog", return_value=[item]), mock.patch.object(
            store_smoke, "fetch", return_value=b"PRG2example"
        ), mock.patch.object(store_smoke.subprocess, "run", return_value=rejection):
            # All-skipped catalogs fail: a green certification requires at least one executed cartridge.
            self.assertEqual(store_smoke.certify("https://store.example", "runner", 300), 1)

    def test_success_requires_media_evidence(self):
        item = {"id": "org.example.game", "version": "1.0.0", "architectures": ["qemu"]}
        execution = subprocess.CompletedProcess([], 0, "game: OK (300 frames)\n", "")
        with mock.patch.object(store_smoke, "catalog", return_value=[item]), mock.patch.object(
            store_smoke, "fetch", return_value=b"PRG2example"
        ), mock.patch.object(store_smoke.subprocess, "run", return_value=execution):
            self.assertEqual(store_smoke.certify("https://store.example", "runner", 300), 1)

    def test_media_evidence_is_reported_as_pass(self):
        item = {"id": "org.example.game", "version": "1.0.0", "architectures": ["qemu"]}
        output = (
            "game: OK (300 frames)\n"
            "MEDIA graphics_non_black=64000 unique_frame_hashes=12 "
            "audio_declared=1 audio_events=4 pcm_samples=2048\n"
        )
        execution = subprocess.CompletedProcess([], 0, output, "")
        with mock.patch.object(store_smoke, "catalog", return_value=[item]), mock.patch.object(
            store_smoke, "fetch", return_value=b"PRG2example"
        ), mock.patch.object(store_smoke.subprocess, "run", return_value=execution):
            self.assertEqual(store_smoke.certify("https://store.example", "runner", 300), 0)


if __name__ == "__main__":
    unittest.main()
