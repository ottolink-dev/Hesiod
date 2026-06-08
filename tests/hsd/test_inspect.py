"""
Tests for scripts/hsd/inspect.py — image analysis functions.
Uses the same PNG encoder from test_png.py to build deterministic images.
"""
import io
import struct
import zlib
import os
import sys
import tempfile
import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "scripts"))

from hsd.inspect import stats, edges, landfrac, profile


# ---------------------------------------------------------------------------
# Re-use the encoder from test_png (copied inline for isolation)
# ---------------------------------------------------------------------------

def _png_chunk(name: bytes, data: bytes) -> bytes:
    crc = zlib.crc32(name + data) & 0xFFFFFFFF
    return struct.pack(">I", len(data)) + name + data + struct.pack(">I", crc)


def _encode_png(width, height, bitdepth, color_type, rows_samples, filter_bytes=None):
    channels = {0: 1, 2: 3, 6: 4}[color_type]
    bps = bitdepth // 8
    if filter_bytes is None:
        filter_bytes = [0] * height

    raw_rows = []
    for row in rows_samples:
        buf = bytearray()
        for s in row:
            if bitdepth == 16:
                buf += struct.pack(">H", s)
            else:
                buf.append(s & 0xFF)
        raw_rows.append(bytes(buf))

    scanlines = bytearray()
    for raw, ftype in zip(raw_rows, filter_bytes):
        scanlines += bytes([ftype]) + raw

    compressed = zlib.compress(bytes(scanlines))
    ihdr_data = struct.pack(">IIBBBBB", width, height, bitdepth, color_type, 0, 0, 0)
    out = b"\x89PNG\r\n\x1a\n"
    out += _png_chunk(b"IHDR", ihdr_data)
    out += _png_chunk(b"IDAT", compressed)
    out += _png_chunk(b"IEND", b"")
    return out


def _make_img(width, height, bitdepth, color_type, rows_samples):
    from hsd.png import read_png
    data = _encode_png(width, height, bitdepth, color_type, rows_samples)
    return read_png(io.BytesIO(data))


def _horiz_gradient_16(width, height):
    """16-bit grayscale, horizontal gradient 0..65535 across width."""
    if width == 1:
        rows = [[0]] * height
    else:
        row = [round(i * 65535 / (width - 1)) for i in range(width)]
        rows = [row[:] for _ in range(height)]
    return _make_img(width, height, 16, 0, rows)


def _constant_img(value, width, height, bitdepth=8):
    """Constant-value 8-bit or 16-bit grayscale."""
    row = [value] * width
    rows = [row[:] for _ in range(height)]
    return _make_img(width, height, bitdepth, 0, rows)


# ---------------------------------------------------------------------------
# stats()
# ---------------------------------------------------------------------------

class TestStats:
    def test_keys(self):
        img = _constant_img(128, 4, 4)
        s = stats(img)
        assert set(s.keys()) == {"width", "height", "bitdepth", "channels", "min", "max", "mean"}

    def test_dimensions_propagated(self):
        img = _constant_img(0, 5, 3)
        s = stats(img)
        assert s["width"] == 5
        assert s["height"] == 3

    def test_constant_zero_normalised(self):
        img = _constant_img(0, 4, 4)
        s = stats(img)
        assert s["min"] == pytest.approx(0.0)
        assert s["max"] == pytest.approx(0.0)
        assert s["mean"] == pytest.approx(0.0)

    def test_constant_max_normalised(self):
        img = _constant_img(255, 4, 4)
        s = stats(img)
        assert s["min"] == pytest.approx(1.0)
        assert s["max"] == pytest.approx(1.0)
        assert s["mean"] == pytest.approx(1.0)

    def test_gradient_min_max_mean(self):
        img = _horiz_gradient_16(101, 1)
        s = stats(img)
        assert s["min"] == pytest.approx(0.0, abs=1e-4)
        assert s["max"] == pytest.approx(1.0, abs=1e-4)
        assert s["mean"] == pytest.approx(0.5, abs=0.02)

    def test_16bit_bitdepth_reported(self):
        img = _horiz_gradient_16(4, 4)
        assert stats(img)["bitdepth"] == 16

    def test_rgb_uses_channel0(self):
        # RGB image: channel 0 = red = 200, channels 1,2 = 0
        row = [200, 0, 0] * 4
        img = _make_img(4, 1, 8, 2, [row])
        s = stats(img)
        assert s["min"] == pytest.approx(200 / 255, abs=1e-5)
        assert s["max"] == pytest.approx(200 / 255, abs=1e-5)


# ---------------------------------------------------------------------------
# edges()
# ---------------------------------------------------------------------------

class TestEdges:
    def test_keys(self):
        img = _constant_img(128, 4, 4)
        e = edges(img)
        assert set(e.keys()) == {"top", "bottom", "left", "right", "lr_match"}

    def test_constant_image_all_same(self):
        img = _constant_img(128, 4, 4)
        e = edges(img)
        assert e["top"] == pytest.approx(128 / 255, abs=1e-5)
        assert e["bottom"] == pytest.approx(128 / 255, abs=1e-5)
        assert e["left"] == pytest.approx(128 / 255, abs=1e-5)
        assert e["right"] == pytest.approx(128 / 255, abs=1e-5)

    def test_constant_lr_match_zero(self):
        img = _constant_img(128, 4, 4)
        e = edges(img)
        assert e["lr_match"] == pytest.approx(0.0, abs=1e-5)

    def test_gradient_left_near_zero(self):
        img = _horiz_gradient_16(101, 4)
        e = edges(img)
        assert e["left"] == pytest.approx(0.0, abs=1e-4)

    def test_gradient_right_near_one(self):
        img = _horiz_gradient_16(101, 4)
        e = edges(img)
        assert e["right"] == pytest.approx(1.0, abs=1e-4)

    def test_gradient_lr_match_near_one(self):
        img = _horiz_gradient_16(101, 4)
        e = edges(img)
        assert e["lr_match"] == pytest.approx(1.0, abs=1e-4)

    def test_gradient_top_bottom_equal(self):
        """Horizontal gradient: every row identical → top == bottom."""
        img = _horiz_gradient_16(101, 4)
        e = edges(img)
        assert e["top"] == pytest.approx(e["bottom"], abs=1e-6)

    def test_wrap_seam_zero(self):
        """Constant image simulating a perfect wrap: lr_match must be 0."""
        img = _constant_img(200, 10, 10)
        assert edges(img)["lr_match"] == pytest.approx(0.0, abs=1e-5)


# ---------------------------------------------------------------------------
# landfrac()
# ---------------------------------------------------------------------------

class TestLandfrac:
    def test_all_above_threshold(self):
        img = _constant_img(255, 4, 4)
        assert landfrac(img) == pytest.approx(1.0)

    def test_all_below_threshold(self):
        img = _constant_img(0, 4, 4)
        assert landfrac(img) == pytest.approx(0.0)

    def test_half_gradient(self):
        """Horizontal gradient 0..65535 over 101 pixels: ~50% above 0.5."""
        img = _horiz_gradient_16(101, 1)
        f = landfrac(img)
        assert f == pytest.approx(0.5, abs=0.02)

    def test_custom_threshold(self):
        # 3/4 of pixels == 255 (value 1.0), 1/4 == 0
        row = [255, 255, 255, 0]
        img = _make_img(4, 1, 8, 0, [row])
        assert landfrac(img, threshold=0.5) == pytest.approx(0.75)

    def test_threshold_boundary(self):
        """Pixel exactly at threshold should count as land."""
        # 128/255 ≈ 0.502 > 0.5 threshold
        img = _constant_img(128, 4, 4)
        f = landfrac(img, threshold=0.5)
        assert f == pytest.approx(1.0)


# ---------------------------------------------------------------------------
# profile()
# ---------------------------------------------------------------------------

class TestProfile:
    def test_row_profile_length(self):
        img = _horiz_gradient_16(10, 20)
        p = profile(img, "row", 5)
        assert len(p) == 5

    def test_col_profile_length(self):
        img = _horiz_gradient_16(10, 20)
        p = profile(img, "col", 8)
        assert len(p) == 8

    def test_col_profile_monotone_increasing_gradient(self):
        """Horizontal gradient → column means should be strictly increasing."""
        img = _horiz_gradient_16(101, 4)
        p = profile(img, "col", 10)
        for a, b in zip(p, p[1:]):
            assert b > a, f"Expected monotone, got {p}"

    def test_col_profile_constant_image(self):
        img = _constant_img(200, 8, 8)
        p = profile(img, "col", 4)
        expected = 200 / 255
        for v in p:
            assert v == pytest.approx(expected, abs=1e-5)

    def test_row_profile_constant_image(self):
        img = _constant_img(100, 8, 8)
        p = profile(img, "row", 4)
        expected = 100 / 255
        for v in p:
            assert v == pytest.approx(expected, abs=1e-5)

    def test_row_profile_gradient_not_monotone(self):
        """Horizontal gradient: row means are all the same (~0.5), not monotone."""
        img = _horiz_gradient_16(101, 101)
        p = profile(img, "row", 10)
        for v in p:
            assert v == pytest.approx(0.5, abs=0.02)

    def test_invalid_axis_raises(self):
        img = _constant_img(0, 4, 4)
        with pytest.raises((ValueError, KeyError, Exception)):
            profile(img, "diagonal", 4)

    def test_row_profile_n1_returns_middle_row(self):
        """
        profile(img, 'row', 1) should return the mean of the MIDDLE row
        (index height//2), not index 0.

        Use a 5-row image where each row has a distinct constant value:
          row 0: all 10,  row 1: all 50,  row 2: all 200,  row 3: all 50,  row 4: all 10
        Middle row = index 2 → mean = 200/255.
        """
        rows = [
            [10] * 4,   # row 0
            [50] * 4,   # row 1
            [200] * 4,  # row 2  ← middle (5//2 = 2)
            [50] * 4,   # row 3
            [10] * 4,   # row 4
        ]
        img = _make_img(4, 5, 8, 0, rows)
        result = profile(img, "row", 1)
        assert len(result) == 1
        assert result[0] == pytest.approx(200 / 255, abs=1e-5), (
            f"profile(img,'row',1) should return middle row mean (200/255≈{200/255:.4f}), "
            f"got {result[0]:.4f}. "
            "This checks that n=1 uses index height//2, not index 0."
        )


# ---------------------------------------------------------------------------
# Optional binary-gated integration test
# ---------------------------------------------------------------------------

HESIOD_BIN = "/home/barrulus/dev/Hesiod/build/bin/hesiod"
HESIOD_AVAILABLE = os.path.isfile(HESIOD_BIN) and os.access(HESIOD_BIN, os.X_OK)

SPEC_PATH = (
    "/home/barrulus/dev/Hesiod/.claude/worktrees/hsd-toolkit-foundation"
    "/.claude/skills/hesiod-generate/reference/specs/heightmap_export.json"
)
SPEC_AVAILABLE = os.path.isfile(SPEC_PATH)


@pytest.mark.skipif(
    not (HESIOD_AVAILABLE and SPEC_AVAILABLE),
    reason="hesiod binary or spec not available",
)
def test_real_16bit_heightmap_decode():
    """
    Render a tiny 64x64 heightmap via the toolkit and verify the PNG decoder
    handles a real OpenCV-written 16-bit PNG correctly.

    Hesiod writes the output PNG relative to its working directory (cwd).
    We set cwd=tmpdir so the PNG lands inside the temp directory.
    """
    import subprocess
    import sys
    import shutil

    with tempfile.TemporaryDirectory() as tmpdir:
        hsd_path = os.path.join(tmpdir, "out.hsd")
        env = {
            **os.environ,
            "HESIOD_BIN": HESIOD_BIN,
            "QT_QPA_PLATFORM": "offscreen",
        }
        scripts_dir = os.path.join(
            "/home/barrulus/dev/Hesiod/.claude/worktrees/hsd-toolkit-foundation",
            "scripts",
        )
        # Write a tiny spec with shape=64,64 so we don't depend on the
        # reference spec's baked-in shape (the binary CLI --shape flag is
        # advisory and may be overridden by the .hsd config).
        tiny_spec_path = os.path.join(tmpdir, "tiny.json")
        import json as _json
        tiny_spec = {
            "config": {"shape": [64, 64], "tiling": [1, 1], "overlap": 0.0},
            "nodes": [
                {"id": "noise", "type": "NoiseFbm", "params": {"kw": [4, 4], "seed": 42}},
                {"id": "exp", "type": "ExportHeightmap"},
            ],
            "links": [["noise.output", "exp.input"]],
            "export": [{"node": "noise", "port": "output", "path": "heightmap.png"}],
        }
        with open(tiny_spec_path, "w") as f_:
            _json.dump(tiny_spec, f_)

        # Step 1: build the .hsd from the tiny spec
        build_result = subprocess.run(
            [
                sys.executable, "-m", "hsd", "build", tiny_spec_path,
                "-o", hsd_path,
            ],
            capture_output=True,
            text=True,
            env={**env, "PYTHONPATH": scripts_dir},
        )
        assert build_result.returncode == 0, (
            f"build failed: {build_result.stderr}\nstdout: {build_result.stdout}"
        )

        # Step 2: run the binary with cwd=tmpdir so PNGs land there.
        # The hesiod binary writes output relative to its working directory.
        run_result = subprocess.run(
            [
                HESIOD_BIN,
                f"--batch={hsd_path}",
            ],
            capture_output=True,
            text=True,
            env=env,
            cwd=tmpdir,
        )

        # Find the produced PNG in tmpdir
        pngs = [f for f in os.listdir(tmpdir) if f.endswith(".png")]
        assert pngs, (
            f"No PNG produced. rc={run_result.returncode}\n"
            f"stdout={run_result.stdout}\nstderr={run_result.stderr}"
        )
        png_path = os.path.join(tmpdir, pngs[0])
        with open(png_path, "rb") as f:
            from hsd.png import read_png
            img = read_png(f)

        assert img["width"] == 64
        assert img["height"] == 64
        assert img["bitdepth"] == 16

        s = stats(img)
        assert 0.0 <= s["min"] <= s["max"] <= 1.0
        assert 0.0 <= s["mean"] <= 1.0
