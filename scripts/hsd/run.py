import os
import subprocess
from pathlib import Path

_REPO_ROOT = Path(__file__).resolve().parents[2]
_DEFAULT_BIN = _REPO_ROOT / "build" / "bin" / "hesiod"


class RunError(Exception):
    pass


def find_binary():
    env = os.environ.get("HESIOD_BIN")
    if env:
        if Path(env).exists():
            return env
        raise RunError(f"HESIOD_BIN is set to {env!r} but that path does not exist")
    if _DEFAULT_BIN.exists():
        return str(_DEFAULT_BIN)
    raise RunError(
        "hesiod binary not found; set HESIOD_BIN or build to build/bin/hesiod")


def build_batch_command(binary, hsd_path, shape=None, tiling=None, overlap=None):
    cmd = [binary, f"--batch={hsd_path}"]
    if shape:
        cmd.append(f"--shape={shape[0]},{shape[1]}")
    if tiling:
        cmd.append(f"--tiling={tiling[0]},{tiling[1]}")
    if overlap is not None:
        cmd.append(f"--overlap={overlap}")
    return cmd


def expected_outputs(hsd_path):
    """Given the export_path in a file, return (raw_png, preview_png) paths."""
    import json
    with open(hsd_path) as f:
        ep = json.load(f)["graph_manager"]["export_param"]
    path = ep.get("export_path", "")
    if not path:
        return (None, None)
    p = Path(path)
    if not p.suffix:
        p = p.with_suffix(".png")
    preview = p.with_name(p.stem + "_preview" + p.suffix)
    return (str(p), str(preview))


def run_batch(hsd_path, shape=None, tiling=None, overlap=None, cwd=None):
    binary = find_binary()
    cmd = build_batch_command(binary, hsd_path, shape, tiling, overlap)
    proc = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    raw, preview = expected_outputs(hsd_path)
    return {
        "returncode": proc.returncode,
        "stdout": proc.stdout,
        "stderr": proc.stderr,
        "raw_png": raw,
        "preview_png": preview,
    }
