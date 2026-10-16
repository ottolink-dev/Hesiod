"""Failure-injection checks: published downloads survive incomplete runs."""
import copy
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import unittest
import zipfile

SPEC = importlib.util.spec_from_file_location(
    "publish_weekly", Path(__file__).resolve().parents[2] / "scripts/ci/publish_weekly.py")
publisher = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(publisher)
SHA = "a" * 40


class Store:
    def __init__(self, fail=None):
        self.manifest = {"version": 2, "channels": {
            "stable": {"linux": {"key": "stable-linux"}},
            "dev": {p: {"key": f"releases/dev/{p}/old.zip"}
                    for p in publisher.ARCHIVES}}}
        self.original = copy.deepcopy(self.manifest)
        self.objects = {}
        self.deleted = []
        self.fail = fail
        self.switched = False
        self.expected_uploads = 2

    def get_object(self, **kwargs):
        return {"Body": io.BytesIO(json.dumps(self.manifest).encode()), "ETag": '"v1"'}

    def upload_file(self, path, bucket, key, ExtraArgs):
        if self.fail == "upload" and "/windows/" in key:
            raise RuntimeError("Upload failed")
        self.objects[key] = {"ContentLength": Path(path).stat().st_size,
                             "Metadata": ExtraArgs["Metadata"]}

    def head_object(self, Bucket, Key):
        if self.fail == "verification":
            return {"ContentLength": 0}
        return self.objects[Key]

    def put_object(self, **kwargs):
        assert len(self.objects) == self.expected_uploads
        assert kwargs["IfMatch"] == '"v1"'
        if self.fail == "conflict":
            raise RuntimeError("PreconditionFailed")
        self.manifest = json.loads(kwargs["Body"])
        self.switched = True

    def delete_object(self, Bucket, Key):
        assert self.switched
        self.deleted.append(Key)


class PublishTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for filename, executable in publisher.ARCHIVES.values():
            with zipfile.ZipFile(self.root / filename, "w") as archive:
                archive.writestr(executable, b"binary")
                archive.writestr("data/settings.json", "{}")
                archive.writestr("SOURCE.txt", SHA + "\n")

    def test_pair_switch_preserves_stable_and_removes_only_old_pair(self):
        store = Store()
        publisher.publish(store, "bucket", self.root, SHA)
        self.assertEqual(store.manifest["channels"]["stable"], store.original["channels"]["stable"])
        self.assertEqual(len(store.deleted), 2)
        for platform in publisher.ARCHIVES:
            self.assertEqual(store.manifest["channels"]["dev"][platform]["source"], SHA)

    def test_failures_leave_previous_downloads_untouched(self):
        for failure in ("upload", "verification", "conflict"):
            with self.subTest(failure=failure):
                store = Store(failure)
                with self.assertRaises(RuntimeError):
                    publisher.publish(store, "bucket", self.root, SHA)
                self.assertEqual(store.manifest, store.original)
                self.assertEqual(store.deleted, [])

    def test_authentication_failure_does_not_initialize_manifest(self):
        store = Store()
        def denied(**kwargs):
            raise PermissionError("Access denied")
        store.get_object = denied
        with self.assertRaises(PermissionError):
            publisher.publish(store, "bucket", self.root, SHA)
        self.assertEqual(store.objects, {})
        self.assertEqual(store.manifest, store.original)

    def test_legacy_manifest_preserves_stable_downloads(self):
        store = Store()
        store.manifest = {"linux": {"key": "legacy-stable"}}
        publisher.publish(store, "bucket", self.root, SHA)
        self.assertEqual(store.manifest["channels"]["stable"]["linux"]["key"],
                         "legacy-stable")
        self.assertEqual(store.deleted, [])

    def test_future_manifest_schema_fails_without_upload(self):
        store = Store()
        store.manifest["version"] = 3
        with self.assertRaises(ValueError):
            publisher.publish(store, "bucket", self.root, SHA)
        self.assertEqual(store.objects, {})

    def test_missing_platform_does_not_upload(self):
        (self.root / publisher.ARCHIVES["windows"][0]).unlink()
        store = Store()
        with self.assertRaises(FileNotFoundError):
            publisher.publish(store, "bucket", self.root, SHA)
        self.assertEqual(store.objects, {})

    def test_mismatched_commit_does_not_upload(self):
        store = Store()
        with self.assertRaises(ValueError):
            publisher.publish(store, "bucket", self.root, "b" * 40)
        self.assertEqual(store.objects, {})


if __name__ == "__main__":
    unittest.main()
