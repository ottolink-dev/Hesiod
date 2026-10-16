"""Publish a complete weekly pair using the website Worker's R2 manifest schema."""
import argparse
import copy
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import uuid
import warnings
import zipfile


ARCHIVES = {
    "linux": ("hesiod-linux-x86_64.zip", "hesiod.AppImage"),
    "windows": ("hesiod-windows-x86_64.zip", "hesiod.exe"),
}


def validate_archives(directory, sha):
    if not re.fullmatch(r"[0-9a-f]{40}", sha):
        raise ValueError("Expected a full source commit SHA")
    archives = {}
    for platform, (filename, executable) in ARCHIVES.items():
        path = directory / filename
        with zipfile.ZipFile(path) as archive:
            names = archive.namelist()
            if executable not in names or not any(n.startswith("data/") for n in names):
                raise ValueError(f"{filename}: executable or runtime data missing")
            if archive.read("SOURCE.txt").decode("utf-8-sig").strip() != sha:
                raise ValueError(f"{filename}: source revision mismatch")
            if archive.testzip() is not None:
                raise ValueError(f"{filename}: corrupt archive")
        with path.open("rb") as stream:
            digest = hashlib.file_digest(stream, "sha256").hexdigest()
        archives[platform] = (path, digest)
    return archives


def publish(client, bucket, directory, sha):
    # Validate BOTH artifacts before touching remote storage.
    archives = validate_archives(directory, sha)
    publish_artifacts(client, bucket, archives, sha)


def publish_artifacts(client, bucket, archives, sha):
    """Publish pre-validated archives, preserving every other platform/channel."""
    try:
        response = client.get_object(Bucket=bucket, Key="manifest.json")
    except Exception as error:
        # Only a missing manifest is safe to initialize; auth/network errors fail.
        if getattr(error, "response", {}).get("Error", {}).get("Code") != "NoSuchKey":
            raise
        manifest = {"version": 2, "channels": {"stable": {}, "dev": {}}}
        condition = {"IfNoneMatch": "*"}
    else:
        manifest = json.loads(response["Body"].read())
        condition = {"IfMatch": response["ETag"]}
        if manifest.get("version") != 2:
            if "version" in manifest:
                raise ValueError("Unsupported website manifest version")
            manifest = {"version": 2, "channels": {"stable": {
                p: manifest[p] for p in ("linux", "windows", "macos") if p in manifest
            }, "dev": {}}}
    previous = copy.deepcopy(manifest)
    dev = manifest["channels"].setdefault("dev", {})
    now = datetime.now(timezone.utc).isoformat()
    build_id = f"{sha}-{uuid.uuid4().hex}"
    for platform, (path, digest) in archives.items():
        key = f"releases/dev/{platform}/{build_id}-{path.name}"
        client.upload_file(str(path), bucket, key, ExtraArgs={
            "ContentType": "application/zip",
            "Metadata": {"sha256": digest, "source": sha},
        })
        uploaded = client.head_object(Bucket=bucket, Key=key)
        if (uploaded["ContentLength"] != path.stat().st_size or
                uploaded.get("Metadata", {}).get("sha256") != digest):
            raise RuntimeError(f"Upload verification failed: {platform}")
        dev[platform] = {
            "key": key, "filename": path.name, "size": path.stat().st_size,
            "version": f"dev-{now[:10]}-{sha[:12]}", "uploaded": now,
            "uploadedBy": "github-actions", "sha256": digest, "source": sha,
        }
    # One conditional write switches both downloads. A concurrent admin update
    # fails this write instead of being overwritten. Do not automatically retry.
    client.put_object(Bucket=bucket, Key="manifest.json",
                      Body=json.dumps(manifest, indent=2).encode(),
                      ContentType="application/json", **condition)
    # Delete only the superseded objects from the snapshot, never an entire
    # prefix: a concurrent upload may already exist under that prefix.
    live_keys = {r["key"] for channel in manifest["channels"].values()
                 for r in channel.values()}
    for platform in archives:
        old = previous["channels"].get("dev", {}).get(platform, {}).get("key")
        if old and old.startswith(f"releases/dev/{platform}/") and old not in live_keys:
            try:
                client.delete_object(Bucket=bucket, Key=old)
            except Exception as error:
                warnings.warn(f"Published successfully; old object cleanup failed: {error}")
    print(f"Published {', '.join(archives)} development builds from {sha}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", type=Path, required=True)
    parser.add_argument("--sha", required=True)
    args = parser.parse_args()
    for name in ("AWS_ACCESS_KEY_ID", "AWS_SECRET_ACCESS_KEY", "R2_ENDPOINT", "R2_BUCKET"):
        if not os.environ.get(name):
            parser.error(f"Missing required website configuration: {name}")
    import boto3
    from botocore.config import Config
    client = boto3.client("s3", endpoint_url=os.environ["R2_ENDPOINT"], region_name="auto",
                          config=Config(request_checksum_calculation="when_required",
                                        response_checksum_validation="when_required",
                                        retries={"max_attempts": 3, "mode": "standard"}))
    publish(client, os.environ["R2_BUCKET"], args.directory, args.sha)


if __name__ == "__main__":
    main()
