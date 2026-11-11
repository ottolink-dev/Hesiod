"""Publish an explicitly selected, successful first-party macOS CI artifact."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import plistlib
import re
import subprocess
import tempfile
import zipfile

from publish_weekly import publish_artifacts

REPOSITORY = "ottolink-dev/Hesiod"
ARCHIVE = "hesiod-macos-arm64.zip"
RESOURCES = "Hesiod.app/Contents/Resources/"


def github(path):
    return json.loads(subprocess.check_output(["gh", "api", f"repos/{REPOSITORY}/{path}"]))


def validate_run(run, head_sha):
    if not re.fullmatch(r"[0-9a-f]{40}", head_sha):
        raise ValueError("Expected a full head commit SHA")
    if (run["status"] != "completed" or run["conclusion"] != "success"
            or run["path"] != ".github/workflows/macos-build.yml"
            or run["head_repository"]["full_name"] != REPOSITORY
            or run["head_sha"] != head_sha):
        raise ValueError("Expected the selected successful first-party macOS build")


def validate_package(directory):
    path = directory / ARCHIVE
    with path.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    checksum = (directory / (ARCHIVE + ".sha256")).read_text().split()
    if checksum != [digest, ARCHIVE]:
        raise ValueError("macOS archive checksum mismatch")
    source = (directory / "SOURCE.txt").read_bytes()
    lines = source.decode().splitlines()
    if (len(lines) < 2 or lines[0] != f"Hesiod source: https://github.com/{REPOSITORY}"
            or not re.fullmatch(r"[0-9a-f]{40}", lines[1])):
        raise ValueError("Invalid macOS source provenance")
    report = json.loads((directory / "verification.json").read_text())
    if (report.get("architecture") != "arm64" or report.get("external_dependencies") != []
            or report.get("mach_o_files", 0) < 1):
        raise ValueError("Invalid macOS dependency audit")
    with zipfile.ZipFile(path) as archive:
        names = archive.namelist()
        if ("Hesiod.app/Contents/MacOS/Hesiod" not in names
                or not any(n.startswith(RESOURCES + "data/") for n in names)
                or RESOURCES + "LICENSE" not in names):
            raise ValueError("macOS app, runtime data or licence missing")
        if archive.read(RESOURCES + "SOURCE.txt") != source:
            raise ValueError("Packaged source provenance does not match")
        info = plistlib.loads(archive.read("Hesiod.app/Contents/Info.plist"))
        minimum = report.get("minimum_macos", "")
        if not re.fullmatch(r"\d+(?:\.\d+){1,2}", minimum):
            raise ValueError("Missing minimum macOS version")
        if info.get("LSMinimumSystemVersion") != minimum:
            raise ValueError("macOS version does not match audit")
        if archive.testzip() is not None:
            raise ValueError("Corrupt macOS archive")
    return lines[1], {"macos": (path, digest)}, report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run-id", type=int, required=True)
    parser.add_argument("--head-sha", required=True)
    args = parser.parse_args()
    run = github(f"actions/runs/{args.run_id}")
    validate_run(run, args.head_sha)
    with tempfile.TemporaryDirectory(prefix="hesiod-macos-publish-") as temp:
        directory = Path(temp)
        subprocess.run(["gh", "run", "download", str(args.run_id), "--repo", REPOSITORY,
                        "--name", "hesiod-macos-arm64", "--dir", temp], check=True)
        source_sha, archives, report = validate_package(directory)
        # Pull-request CI builds a synthetic merge commit. Verify its relationship
        # to the explicitly approved head rather than mislabelling the source.
        if source_sha != args.head_sha:
            commit = github(f"commits/{source_sha}")
            parents = commit["parents"]
            if (run["event"] != "pull_request" or len(parents) != 2
                    or parents[1]["sha"] != args.head_sha):
                raise ValueError("Package source is not the selected build's PR merge")
        print(f"Validated {source_sha}: {json.dumps(report, sort_keys=True)}", flush=True)
        import boto3
        from botocore.config import Config
        client = boto3.client("s3", endpoint_url=os.environ["R2_ENDPOINT"], region_name="auto",
                              config=Config(request_checksum_calculation="when_required",
                                            response_checksum_validation="when_required",
                                            retries={"max_attempts": 3, "mode": "standard"}))
        publish_artifacts(client, os.environ["R2_BUCKET"], archives, source_sha)


if __name__ == "__main__":
    main()
