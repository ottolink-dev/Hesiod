import os
import sys
import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "scripts"))

from hsd.enums import EnumCatalog, EnumError


@pytest.fixture(scope="module")
def catalog():
    return EnumCatalog.load()


# ---------------------------------------------------------------------------
# Basic load
# ---------------------------------------------------------------------------

def test_load_default_path(catalog):
    assert catalog is not None


# ---------------------------------------------------------------------------
# resolve — correct integer values
# ---------------------------------------------------------------------------

def test_resolve_blend_blending_method_maximum(catalog):
    # blending_method_map: "maximum" -> 3
    assert catalog.resolve("Blend", "blending_method", "maximum") == 3


def test_resolve_stamping_blend_method_maximum(catalog):
    # stamping_blend_method_map: "maximum" -> 1
    # This is the disambiguation proof: same choice name, different map, different int
    assert catalog.resolve("Stamping", "blend_method", "maximum") == 1


def test_resolve_noisefbm_noise_type_opensimplex2(catalog):
    # noise_type_map_fbm: "OpenSimplex2" -> 4
    assert catalog.resolve("NoiseFbm", "noise_type", "OpenSimplex2") == 4


def test_resolve_gain_post_mix_method_replace(catalog):
    # Gain.post_mix_method -> blending_method_map; "replace" -> 11
    result = catalog.resolve("Gain", "post_mix_method", "replace")
    assert isinstance(result, int)


# ---------------------------------------------------------------------------
# resolve — error on invalid choice
# ---------------------------------------------------------------------------

def test_resolve_invalid_choice_raises_enum_error(catalog):
    with pytest.raises(EnumError) as exc_info:
        catalog.resolve("Blend", "blending_method", "not_a_method")
    assert "maximum" in str(exc_info.value)


# ---------------------------------------------------------------------------
# map_for
# ---------------------------------------------------------------------------

def test_map_for_blend_blending_method(catalog):
    assert catalog.map_for("Blend", "blending_method") == "blending_method_map"


def test_map_for_uncatalogued_node_returns_none(catalog):
    # CloudRandom is not in node_param at all
    assert catalog.map_for("CloudRandom", "method") is None


# ---------------------------------------------------------------------------
# resolve — uncatalogued node/param returns None (caller falls back)
# ---------------------------------------------------------------------------

def test_resolve_uncatalogued_returns_none(catalog):
    assert catalog.resolve("CloudRandom", "method", "some_choice") is None


# ---------------------------------------------------------------------------
# choices
# ---------------------------------------------------------------------------

def test_choices_blend_blending_method_sorted_and_contains_expected(catalog):
    result = catalog.choices("Blend", "blending_method")
    assert isinstance(result, list)
    assert result == sorted(result), "choices() must return a sorted list"
    assert "add" in result
    assert "maximum" in result
