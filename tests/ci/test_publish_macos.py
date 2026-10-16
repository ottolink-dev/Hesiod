"""Mac publication must never change the Windows/Linux or stable downloads."""
import hashlib
import json
from pathlib import Path
import plistlib
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts/ci"))
from publish_macos import ARCHIVE, REPOSITORY, RESOURCES, validate_package, validate_run
from publish_weekly import publish_artifacts
from test_publish_weekly import SHA, Store


class MacPublishTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.directory = Path(temp.name)
        self.source = f"Hesiod source: https://github.com/{REPOSITORY}\n{SHA}\n"
        (self.directory / "SOURCE.txt").write_text(self.source)
        (self.directory / "verification.json").write_text(json.dumps({
            "architecture": "arm64", "mach_o_files": 99,
            "external_dependencies": [], "minimum_macos": "14.0"}))
        with zipfile.ZipFile(self.directory / ARCHIVE, "w") as z:
            z.writestr("Hesiod.app/Contents/MacOS/Hesiod", b"binary")
            z.writestr(RESOURCES + "data/settings.json", "{}")
            z.writestr(RESOURCES + "LICENSE", "GPL-3.0")
            z.writestr(RESOURCES + "SOURCE.txt", self.source)
            z.writestr("Hesiod.app/Contents/Info.plist", plistlib.dumps({
                "LSMinimumSystemVersion": "14.0"}))
        self.digest = hashlib.sha256((self.directory / ARCHIVE).read_bytes()).hexdigest()
        (self.directory / (ARCHIVE + ".sha256")).write_text(f"{self.digest}  {ARCHIVE}\n")

    def test_publish_preserves_all_existing_channels_and_platforms(self):
        sha, archives, report = validate_package(self.directory)
        self.assertEqual(sha, SHA)
        store = Store()
        store.expected_uploads = 1
        publish_artifacts(store, "bucket", archives, sha)
        for channel, platforms in store.original["channels"].items():
            for platform, value in platforms.items():
                self.assertEqual(store.manifest["channels"][channel][platform], value)
        self.assertEqual(store.manifest["channels"]["dev"]["macos"]["sha256"], self.digest)
        self.assertEqual(store.deleted, [])

    def test_failed_upload_or_manifest_conflict_keeps_downloads(self):
        sha, archives, _ = validate_package(self.directory)
        for failure in ("verification", "conflict"):
            store = Store(failure)
            store.expected_uploads = 1
            with self.assertRaises(RuntimeError):
                publish_artifacts(store, "bucket", archives, sha)
            self.assertEqual(store.manifest, store.original)
            self.assertEqual(store.deleted, [])

    def test_bad_checksum_is_rejected(self):
        (self.directory / (ARCHIVE + ".sha256")).write_text(f"{'0' * 64}  {ARCHIVE}\n")
        with self.assertRaisesRegex(ValueError, "checksum"):
            validate_package(self.directory)

    def test_source_sidecar_mismatch_is_rejected(self):
        (self.directory / "SOURCE.txt").write_text(self.source.replace(SHA, "b" * 40))
        with self.assertRaisesRegex(ValueError, "provenance does not match"):
            validate_package(self.directory)

    def test_unexpected_audit_is_rejected(self):
        path = self.directory / "verification.json"
        original = json.loads(path.read_text())
        for change in ({"architecture": "x86_64"}, {"external_dependencies": ["/opt/homebrew/lib/x"]},
                       {"minimum_macos": "15.0"}):
            path.write_text(json.dumps(original | change))
            with self.assertRaises(ValueError):
                validate_package(self.directory)

    def test_wrong_run_is_rejected(self):
        run = {"status": "completed", "conclusion": "success", "head_sha": SHA,
               "path": ".github/workflows/macos-build.yml",
               "head_repository": {"full_name": REPOSITORY}}
        validate_run(run, SHA)
        for change in ({"conclusion": "failure"}, {"head_sha": "b" * 40},
                       {"path": ".github/workflows/other.yml"},
                       {"head_repository": {"full_name": "someone/fork"}}):
            with self.assertRaises(ValueError):
                validate_run(run | change, SHA)
