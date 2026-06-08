"""
Numerical analysis of decoded PNG images (heightmaps).

All functions operate on a single luminance channel:
  - Grayscale images (channels=1): the gray sample.
  - RGB/RGBA images (channels>=3): channel 0 (red channel is used as luminance).

Samples are normalised to [0.0, 1.0] by dividing by the maximum representable
value for the bit depth (255 for 8-bit, 65535 for 16-bit).

The ``img`` argument accepted by all functions is the dict returned by
:func:`hsd.png.read_png`.
"""

__all__ = ["stats", "edges", "landfrac", "profile"]


# ---------------------------------------------------------------------------
# Internal helpers
# ---------------------------------------------------------------------------

def _max_value(bitdepth: int) -> int:
    """Maximum integer sample value for a given bit depth."""
    return (1 << bitdepth) - 1


def _luma_rows(img) -> list:
    """
    Return a list of rows where each row is a list of normalised (0..1) floats
    taken from the luminance channel.
    """
    bitdepth = img["bitdepth"]
    channels = img["channels"]
    maxval = _max_value(bitdepth)
    rows = img["rows"]

    if channels == 1:
        # Grayscale: one sample per pixel
        return [[s / maxval for s in row] for row in rows]
    else:
        # RGB or RGBA: use channel 0 (stride = channels)
        return [
            [row[i * channels] / maxval for i in range(img["width"])]
            for row in rows
        ]


# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------

def stats(img: dict) -> dict:
    """
    Return summary statistics for the image.

    Returns a dict with keys:
      ``width``, ``height``, ``bitdepth``, ``channels``
        Passed through from the decoded image.
      ``min``, ``max``, ``mean``
        Normalised 0..1 float values over the luminance channel.
    """
    lrows = _luma_rows(img)
    flat = [v for row in lrows for v in row]
    n = len(flat)
    mn = min(flat) if flat else 0.0
    mx = max(flat) if flat else 0.0
    mean = sum(flat) / n if n else 0.0
    return {
        "width": img["width"],
        "height": img["height"],
        "bitdepth": img["bitdepth"],
        "channels": img["channels"],
        "min": mn,
        "max": mx,
        "mean": mean,
    }


def edges(img: dict) -> dict:
    """
    Return mean normalised values along each border and a left-right seam metric.

    Returns a dict with keys:
      ``top``
        Mean of the top border row.
      ``bottom``
        Mean of the bottom border row.
      ``left``
        Mean of the leftmost column.
      ``right``
        Mean of the rightmost column.
      ``lr_match``
        Maximum absolute difference between any pixel in the first column and
        the corresponding pixel in the last column (normalised).  Use this to
        check wrap-seam quality: a value near 0 means the left and right edges
        match well; near 1 means they are completely different.
    """
    lrows = _luma_rows(img)
    height = img["height"]
    width = img["width"]

    top_mean = sum(lrows[0]) / width
    bottom_mean = sum(lrows[-1]) / width

    left_col = [lrows[y][0] for y in range(height)]
    right_col = [lrows[y][-1] for y in range(height)]

    left_mean = sum(left_col) / height
    right_mean = sum(right_col) / height

    lr_match = max(abs(left_col[y] - right_col[y]) for y in range(height))

    return {
        "top": top_mean,
        "bottom": bottom_mean,
        "left": left_mean,
        "right": right_mean,
        "lr_match": lr_match,
    }


def landfrac(img: dict, threshold: float = 0.5) -> float:
    """
    Return the fraction of pixels whose normalised value is >= *threshold*.

    Parameters
    ----------
    img:
        Decoded image dict from :func:`hsd.png.read_png`.
    threshold:
        Normalised value (0..1) above which a pixel is counted as "land".
        Default 0.5.

    Returns
    -------
    float in [0.0, 1.0].
    """
    lrows = _luma_rows(img)
    flat = [v for row in lrows for v in row]
    if not flat:
        return 0.0
    count = sum(1 for v in flat if v >= threshold)
    return count / len(flat)


def profile(img: dict, axis: str, n: int) -> list:
    """
    Return *n* evenly-spaced mean values along the given axis.

    Parameters
    ----------
    img:
        Decoded image dict from :func:`hsd.png.read_png`.
    axis:
        ``"row"`` — *n* evenly-spaced rows from top to bottom; each value is
        the mean (normalised) of all pixels in that row.

        ``"col"`` — *n* evenly-spaced columns from left to right; each value
        is the mean (normalised) of all pixels in that column.
    n:
        Number of samples to return.

    Returns
    -------
    list of *n* floats.

    Raises
    ------
    ValueError
        If *axis* is not ``"row"`` or ``"col"``.
    """
    if axis not in ("row", "col"):
        raise ValueError(f"axis must be 'row' or 'col', got {axis!r}")

    lrows = _luma_rows(img)
    height = img["height"]
    width = img["width"]

    if axis == "row":
        total = height
        def _mean_at(i):
            return sum(lrows[i]) / width
    else:
        total = width
        def _mean_at(i):
            return sum(lrows[y][i] for y in range(height)) / height

    if n == 1:
        indices = [0]
    else:
        # n evenly-spaced indices spanning [0, total-1]
        indices = [round(i * (total - 1) / (n - 1)) for i in range(n)]

    return [_mean_at(idx) for idx in indices]
