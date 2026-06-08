"""
Tests for scripts/hsd/png.py — stdlib-only PNG reader.

A minimal PNG encoder is embedded here to produce deterministic test images
without any external dependencies (PIL/numpy/etc).
"""
import io
import os
import struct
import sys
import zlib
import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "scripts"))

from hsd.png import read_png


# ---------------------------------------------------------------------------
# Minimal PNG encoder (test-only) — pure stdlib
# ---------------------------------------------------------------------------

def _png_chunk(name: bytes, data: bytes) -> bytes:
    crc = zlib.crc32(name + data) & 0xFFFFFFFF
    return struct.pack(">I", len(data)) + name + data + struct.pack(">I", crc)


def _encode_png(width, height, bitdepth, color_type, rows_samples, filter_bytes=None):
    """
    Encode a PNG from raw samples.

    rows_samples: list of rows; each row is a list of integer samples
                  (length = width * channels).
    filter_bytes: list of per-row filter type bytes (0..4). None → all 0.
    bitdepth: 8 or 16
    color_type: 0 (gray), 2 (RGB), 6 (RGBA)
    """
    channels = {0: 1, 2: 3, 6: 4}[color_type]
    bps = bitdepth // 8  # bytes per sample (1 for 8-bit, 2 for 16-bit)
    bpp = channels * bps  # bytes per pixel — look-back stride per PNG spec
    if filter_bytes is None:
        filter_bytes = [0] * height

    # Build raw (unfiltered) scanlines first
    raw_rows = []
    for row in rows_samples:
        buf = bytearray()
        for s in row:
            if bitdepth == 16:
                buf += struct.pack(">H", s)
            else:
                buf.append(s & 0xFF)
        raw_rows.append(bytes(buf))

    # Apply filters
    stride = width * channels * bps

    def sub_filter(raw, prev):
        out = bytearray(len(raw))
        for i in range(len(raw)):
            a = raw[i - bpp] if i >= bpp else 0
            out[i] = (raw[i] - a) & 0xFF
        return bytes(out)

    def up_filter(raw, prev):
        out = bytearray(len(raw))
        for i in range(len(raw)):
            b = prev[i] if prev else 0
            out[i] = (raw[i] - b) & 0xFF
        return bytes(out)

    def avg_filter(raw, prev):
        out = bytearray(len(raw))
        for i in range(len(raw)):
            a = raw[i - bpp] if i >= bpp else 0
            b = prev[i] if prev else 0
            out[i] = (raw[i] - ((a + b) >> 1)) & 0xFF
        return bytes(out)

    def paeth_predictor(a, b, c):
        p = a + b - c
        pa = abs(p - a)
        pb = abs(p - b)
        pc = abs(p - c)
        if pa <= pb and pa <= pc:
            return a
        if pb <= pc:
            return b
        return c

    def paeth_filter(raw, prev):
        out = bytearray(len(raw))
        for i in range(len(raw)):
            a = raw[i - bpp] if i >= bpp else 0
            b = prev[i] if prev else 0
            c = prev[i - bpp] if (prev and i >= bpp) else 0
            out[i] = (raw[i] - paeth_predictor(a, b, c)) & 0xFF
        return bytes(out)

    scanlines = bytearray()
    prev = None
    for i, (raw, ftype) in enumerate(zip(raw_rows, filter_bytes)):
        if ftype == 0:
            filtered = raw
        elif ftype == 1:
            filtered = sub_filter(raw, prev)
        elif ftype == 2:
            filtered = up_filter(raw, prev)
        elif ftype == 3:
            filtered = avg_filter(raw, prev)
        elif ftype == 4:
            filtered = paeth_filter(raw, prev)
        else:
            raise ValueError(f"Unknown filter type {ftype}")
        scanlines += bytes([ftype]) + filtered
        prev = raw

    compressed = zlib.compress(bytes(scanlines))

    ihdr_data = struct.pack(">IIBBBBB", width, height, bitdepth, color_type, 0, 0, 0)
    out = b"\x89PNG\r\n\x1a\n"
    out += _png_chunk(b"IHDR", ihdr_data)
    out += _png_chunk(b"IDAT", compressed)
    out += _png_chunk(b"IEND", b"")
    return out


# ---------------------------------------------------------------------------
# Helper
# ---------------------------------------------------------------------------

def _from_bytes(data):
    return read_png(io.BytesIO(data))


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

class TestGray8RoundTrip:
    """2x2 grayscale 8-bit, filter type 0 (None)."""

    def test_dimensions(self):
        png = _encode_png(2, 2, 8, 0, [[10, 20], [30, 40]])
        img = _from_bytes(png)
        assert img["width"] == 2
        assert img["height"] == 2

    def test_bitdepth_and_channels(self):
        png = _encode_png(2, 2, 8, 0, [[10, 20], [30, 40]])
        img = _from_bytes(png)
        assert img["bitdepth"] == 8
        assert img["channels"] == 1

    def test_samples_exact(self):
        png = _encode_png(2, 2, 8, 0, [[10, 20], [30, 40]])
        img = _from_bytes(png)
        assert img["rows"][0] == [10, 20]
        assert img["rows"][1] == [30, 40]


class TestGray16RoundTrip:
    """16-bit grayscale round-trip; verify big-endian handling."""

    def test_bitdepth(self):
        png = _encode_png(2, 1, 16, 0, [[65535, 256]])
        img = _from_bytes(png)
        assert img["bitdepth"] == 16

    def test_sample_max(self):
        png = _encode_png(2, 1, 16, 0, [[65535, 256]])
        img = _from_bytes(png)
        assert img["rows"][0][0] == 65535

    def test_sample_256(self):
        """256 = 0x0100; big-endian: 0x01 then 0x00. Must NOT be read as 1."""
        png = _encode_png(2, 1, 16, 0, [[65535, 256]])
        img = _from_bytes(png)
        assert img["rows"][0][1] == 256

    def test_sample_zero(self):
        png = _encode_png(3, 1, 16, 0, [[0, 32768, 65535]])
        img = _from_bytes(png)
        assert img["rows"][0] == [0, 32768, 65535]


class TestAllFilterTypes:
    """
    Exercise each of the five PNG scanline filters (0-4) on distinct rows.
    Use a 4x4 grayscale 8-bit image with known pixel values, encoding each
    row with a different filter type, then assert exact round-trip.
    """

    WIDTH = 4
    HEIGHT = 5
    # Row samples chosen so they vary enough for filters to do real work
    ROWS = [
        [100, 110, 120, 130],  # row 0 — filter 0 (None)
        [100, 110, 120, 130],  # row 1 — filter 1 (Sub)
        [100, 110, 120, 130],  # row 2 — filter 2 (Up)
        [80,  90, 100, 110],   # row 3 — filter 3 (Average)
        [60,  70,  80,  90],   # row 4 — filter 4 (Paeth)
    ]
    FILTERS = [0, 1, 2, 3, 4]

    @pytest.fixture
    def img(self):
        png = _encode_png(
            self.WIDTH, self.HEIGHT, 8, 0, self.ROWS, filter_bytes=self.FILTERS
        )
        return _from_bytes(png)

    def test_filter0_none(self, img):
        assert img["rows"][0] == self.ROWS[0]

    def test_filter1_sub(self, img):
        assert img["rows"][1] == self.ROWS[1]

    def test_filter2_up(self, img):
        assert img["rows"][2] == self.ROWS[2]

    def test_filter3_average(self, img):
        assert img["rows"][3] == self.ROWS[3]

    def test_filter4_paeth(self, img):
        assert img["rows"][4] == self.ROWS[4]


class TestRGBRoundTrip:
    """RGB 8-bit (color_type=2)."""

    def test_channels(self):
        png = _encode_png(2, 1, 8, 2, [[255, 0, 0, 0, 255, 0]])
        img = _from_bytes(png)
        assert img["channels"] == 3

    def test_samples(self):
        png = _encode_png(2, 1, 8, 2, [[255, 0, 0, 0, 255, 0]])
        img = _from_bytes(png)
        assert img["rows"][0] == [255, 0, 0, 0, 255, 0]


class TestRGBARoundTrip:
    """RGBA 8-bit (color_type=6)."""

    def test_channels(self):
        png = _encode_png(1, 1, 8, 6, [[128, 64, 32, 255]])
        img = _from_bytes(png)
        assert img["channels"] == 4

    def test_samples(self):
        png = _encode_png(1, 1, 8, 6, [[128, 64, 32, 255]])
        img = _from_bytes(png)
        assert img["rows"][0] == [128, 64, 32, 255]


class TestRGBSubFilterCorrect:
    """
    RGB 8-bit image encoded with filter type 1 (Sub), decoded against
    hand-computed expected samples.  This test is INDEPENDENT of the encoder:
    we construct the filtered scanline bytes by hand and parse them directly.

    2×1 image, pixels: (10, 20, 30) and (40, 50, 60).
    Raw scanline bytes: [10, 20, 30, 40, 50, 60]
    bpp = 3 (RGB 8-bit).

    Sub filter encodes:
      i=0: 10 - 0   =  10
      i=1: 20 - 0   =  20
      i=2: 30 - 0   =  30
      i=3: 40 - 10  =  30   (a = raw[i-bpp] = raw[0] = 10)
      i=4: 50 - 20  =  30
      i=5: 60 - 30  =  30
    → filtered scanline: [10, 20, 30, 30, 30, 30]

    A wrong bps=1 decoder computes at i=3: a=out[2]=30 → 30+30=60 ≠ 40.
    """

    # Pre-built PNG with the Sub-filtered scanline above, constructed by hand.
    # Easier to just use the (corrected) encoder and also independently verify.

    def _hand_built_rgb_sub_png(self):
        """
        Build a 2×1 RGB PNG whose single scanline uses filter 1 (Sub).
        The filtered payload bytes are computed above (independent of encoder).
        """
        filtered_payload = bytes([10, 20, 30, 30, 30, 30])
        scanline = bytes([1]) + filtered_payload  # filter_byte=1, then data
        compressed = zlib.compress(scanline)

        ihdr_data = struct.pack(">IIBBBBB", 2, 1, 8, 2, 0, 0, 0)
        png = b"\x89PNG\r\n\x1a\n"
        png += _png_chunk(b"IHDR", ihdr_data)
        png += _png_chunk(b"IDAT", compressed)
        png += _png_chunk(b"IEND", b"")
        return png

    def test_rgb_sub_exact_samples_hand_built(self):
        """Decode a hand-built Sub-filtered RGB PNG; verify exact pixel values."""
        png = self._hand_built_rgb_sub_png()
        img = _from_bytes(png)
        assert img["rows"][0] == [10, 20, 30, 40, 50, 60], (
            f"Wrong decode: got {img['rows'][0]}, expected [10,20,30,40,50,60]. "
            "This catches the bps-vs-bpp look-back bug for RGB Sub filter."
        )

    def test_rgb_sub_via_encoder(self):
        """Round-trip via encoder (both sides fixed) — complementary to hand-built."""
        rows = [[10, 20, 30, 40, 50, 60]]
        png = _encode_png(2, 1, 8, 2, rows, filter_bytes=[1])
        img = _from_bytes(png)
        assert img["rows"][0] == [10, 20, 30, 40, 50, 60]


class TestRGBAPaethFilterCorrect:
    """
    RGBA 8-bit image encoded with filter type 4 (Paeth).
    2×1 image, pixels: (10, 20, 30, 255) and (50, 70, 90, 200).

    Raw bytes: [10, 20, 30, 255, 50, 70, 90, 200]
    bpp = 4 (RGBA).  No previous row → b=c=0.

    Paeth(a,0,0) = a (since pa=0 ≤ pb=b, pc=c when a=b=c=0 for first pixel;
    and for subsequent bytes a is the corresponding reconstructed byte bpp ago).

    i=0..3: a=0, b=0, c=0 → pred=0 → filtered[i] = raw[i]
      filtered[0]=10, filtered[1]=20, filtered[2]=30, filtered[3]=255
    i=4: a=out[0]=10, b=0, c=0 → pred=10 → filtered[4]=50-10=40
    i=5: a=out[1]=20 → filtered[5]=70-20=50
    i=6: a=out[2]=30 → filtered[6]=90-30=60
    i=7: a=out[3]=255 → filtered[7]=(200-255)&0xFF=201

    A wrong bps=1 decoder at i=4: a=out[3]=255 → pred=255 → 40+255=39 ≠ 50.
    """

    def _hand_built_rgba_paeth_png(self):
        filtered_payload = bytes([10, 20, 30, 255, 40, 50, 60, 201])
        scanline = bytes([4]) + filtered_payload  # filter_byte=4
        compressed = zlib.compress(scanline)

        ihdr_data = struct.pack(">IIBBBBB", 2, 1, 8, 6, 0, 0, 0)
        png = b"\x89PNG\r\n\x1a\n"
        png += _png_chunk(b"IHDR", ihdr_data)
        png += _png_chunk(b"IDAT", compressed)
        png += _png_chunk(b"IEND", b"")
        return png

    def test_rgba_paeth_exact_samples_hand_built(self):
        """Decode a hand-built Paeth-filtered RGBA PNG; verify exact pixel values."""
        png = self._hand_built_rgba_paeth_png()
        img = _from_bytes(png)
        assert img["rows"][0] == [10, 20, 30, 255, 50, 70, 90, 200], (
            f"Wrong decode: got {img['rows'][0]}, "
            "expected [10,20,30,255,50,70,90,200]. "
            "This catches the bps-vs-bpp look-back bug for RGBA Paeth filter."
        )

    def test_rgba_paeth_via_encoder(self):
        """Round-trip via encoder (both sides fixed)."""
        rows = [[10, 20, 30, 255, 50, 70, 90, 200]]
        png = _encode_png(2, 1, 8, 6, rows, filter_bytes=[4])
        img = _from_bytes(png)
        assert img["rows"][0] == [10, 20, 30, 255, 50, 70, 90, 200]


class TestUnsupportedTypes:
    """Unsupported colour types / interlacing must raise, not silently mis-decode."""

    def _make_raw_ihdr(self, width, height, bitdepth, color_type, interlace=0):
        ihdr_data = struct.pack(">IIBBBBB", width, height, bitdepth, color_type, 0, 0, interlace)
        return b"\x89PNG\r\n\x1a\n" + _png_chunk(b"IHDR", ihdr_data) + _png_chunk(b"IEND", b"")

    def test_palette_type_raises(self):
        raw = self._make_raw_ihdr(1, 1, 8, 3)  # palette
        with pytest.raises(Exception, match=r"(?i)(palette|unsupported|color.?type)"):
            _from_bytes(raw)

    def test_interlaced_raises(self):
        raw = self._make_raw_ihdr(1, 1, 8, 0, interlace=1)
        with pytest.raises(Exception, match=r"(?i)(interlac|unsupported)"):
            _from_bytes(raw)
