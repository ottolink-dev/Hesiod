"""
Minimal stdlib-only PNG reader.

Supports:
  - Grayscale 8-bit  (color type 0, bitdepth 8)
  - Grayscale 16-bit (color type 0, bitdepth 16)
  - RGB 8-bit        (color type 2, bitdepth 8)
  - RGBA 8-bit       (color type 6, bitdepth 8)

All five PNG scanline filter types are implemented (0-None, 1-Sub, 2-Up,
3-Average, 4-Paeth) because OpenCV-written PNGs (which Hesiod produces) use
adaptive per-scanline filtering.

16-bit samples are decoded big-endian as required by the PNG specification.

Unsupported configurations (palette/indexed colour type 3, interlaced images,
gray-alpha colour type 4) raise ValueError with a clear message.

Usage::

    from hsd.png import read_png

    with open("heightmap.png", "rb") as f:
        img = read_png(f)

    # img keys: width, height, bitdepth, channels
    # img["rows"]: list of rows; each row is a flat list of integer samples
    #              (length = width * channels)

read_png also accepts any file-like object supporting .read(), or a path string.
"""

import struct
import zlib

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"

# Color type → number of channels
_COLOR_TYPE_CHANNELS = {
    0: 1,  # grayscale
    2: 3,  # RGB
    6: 4,  # RGBA
}


def _read_chunk(f):
    """Read one PNG chunk; return (chunk_type_bytes, data_bytes)."""
    length_bytes = f.read(4)
    if len(length_bytes) < 4:
        raise ValueError("Truncated PNG: cannot read chunk length")
    (length,) = struct.unpack(">I", length_bytes)
    chunk_type = f.read(4)
    if len(chunk_type) < 4:
        raise ValueError("Truncated PNG: cannot read chunk type")
    data = f.read(length)
    _crc = f.read(4)  # read but don't verify for speed
    return chunk_type, data


def _paeth_predictor(a, b, c):
    p = a + b - c
    pa = abs(p - a)
    pb = abs(p - b)
    pc = abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def _unfilter(filter_type, raw, prev, bps):
    """
    Undo a single PNG scanline filter.

    raw       : bytes of the filtered scanline (without the filter byte)
    prev      : bytes of the *reconstructed* previous scanline, or None
    bps       : bytes per sample (1 for 8-bit, 2 for 16-bit)
    Returns   : bytearray of reconstructed scanline bytes
    """
    n = len(raw)
    out = bytearray(n)

    if filter_type == 0:  # None
        out[:] = raw

    elif filter_type == 1:  # Sub
        for i in range(n):
            a = out[i - bps] if i >= bps else 0
            out[i] = (raw[i] + a) & 0xFF

    elif filter_type == 2:  # Up
        for i in range(n):
            b = prev[i] if prev is not None else 0
            out[i] = (raw[i] + b) & 0xFF

    elif filter_type == 3:  # Average
        for i in range(n):
            a = out[i - bps] if i >= bps else 0
            b = prev[i] if prev is not None else 0
            out[i] = (raw[i] + ((a + b) >> 1)) & 0xFF

    elif filter_type == 4:  # Paeth
        for i in range(n):
            a = out[i - bps] if i >= bps else 0
            b = prev[i] if prev is not None else 0
            c = prev[i - bps] if (prev is not None and i >= bps) else 0
            out[i] = (raw[i] + _paeth_predictor(a, b, c)) & 0xFF

    else:
        raise ValueError(f"Unknown PNG filter type: {filter_type}")

    return out


def read_png(source):
    """
    Decode a PNG from *source* (file-like object or path string).

    Returns a dict::

        {
            "width":    int,
            "height":   int,
            "bitdepth": int,   # 8 or 16
            "channels": int,   # 1, 3, or 4
            "rows":     list[list[int]],
            # rows[y] is a flat list of integer samples, length = width*channels
            # samples are in natural order: gray / R G B / R G B A
        }

    Raises ValueError for unsupported or corrupt PNG data.
    """
    if isinstance(source, (str, bytes)):
        with open(source, "rb") as f:
            return read_png(f)

    # Read signature
    sig = source.read(8)
    if sig != PNG_SIGNATURE:
        raise ValueError(f"Not a PNG file (bad signature: {sig!r})")

    ihdr = None
    idat_chunks = []

    while True:
        chunk_type, data = _read_chunk(source)
        if chunk_type == b"IHDR":
            if len(data) < 13:
                raise ValueError("Truncated IHDR chunk")
            width, height, bitdepth, color_type, compress, filt, interlace = \
                struct.unpack(">IIBBBBB", data[:13])
            ihdr = {
                "width": width,
                "height": height,
                "bitdepth": bitdepth,
                "color_type": color_type,
                "interlace": interlace,
            }
        elif chunk_type == b"IDAT":
            idat_chunks.append(data)
        elif chunk_type == b"IEND":
            break
        # All other chunks (tEXt, gAMA, etc.) are silently skipped

    if ihdr is None:
        raise ValueError("PNG missing IHDR chunk")

    width = ihdr["width"]
    height = ihdr["height"]
    bitdepth = ihdr["bitdepth"]
    color_type = ihdr["color_type"]
    interlace = ihdr["interlace"]

    # Validation
    if interlace != 0:
        raise ValueError(
            f"Interlaced PNG is not supported (interlace={interlace}). "
            "Re-export without interlacing."
        )
    if color_type not in _COLOR_TYPE_CHANNELS:
        raise ValueError(
            f"Unsupported PNG color type {color_type}. "
            "Supported: 0 (grayscale), 2 (RGB), 6 (RGBA). "
            "Palette (type 3) and gray-alpha (type 4) are not supported."
        )
    if bitdepth not in (8, 16):
        raise ValueError(
            f"Unsupported PNG bit depth {bitdepth}. Only 8 and 16 are supported."
        )

    channels = _COLOR_TYPE_CHANNELS[color_type]
    bps = bitdepth // 8  # bytes per sample

    # Decompress all IDAT chunks together
    raw_idat = b"".join(idat_chunks)
    try:
        scanline_data = zlib.decompress(raw_idat)
    except zlib.error as exc:
        raise ValueError(f"PNG IDAT decompression failed: {exc}") from exc

    stride = width * channels * bps  # bytes per scanline (excluding filter byte)
    expected_len = height * (1 + stride)
    if len(scanline_data) < expected_len:
        raise ValueError(
            f"Decompressed PNG data too short: got {len(scanline_data)} bytes, "
            f"expected {expected_len}"
        )

    rows = []
    prev_reconstructed = None
    pos = 0
    for _y in range(height):
        filter_type = scanline_data[pos]
        pos += 1
        raw = scanline_data[pos:pos + stride]
        pos += stride

        reconstructed = _unfilter(filter_type, raw, prev_reconstructed, bps)
        prev_reconstructed = bytes(reconstructed)

        # Parse samples from reconstructed bytes
        if bps == 1:
            row_samples = list(reconstructed)
        else:  # bps == 2, big-endian uint16
            n_samples = width * channels
            row_samples = [
                (reconstructed[i * 2] << 8) | reconstructed[i * 2 + 1]
                for i in range(n_samples)
            ]

        rows.append(row_samples)

    return {
        "width": width,
        "height": height,
        "bitdepth": bitdepth,
        "channels": channels,
        "rows": rows,
    }
