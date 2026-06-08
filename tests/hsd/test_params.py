import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "scripts"))

import pytest
from hsd.params import value_object, ParamError
from hsd.enums import EnumCatalog, EnumError


def test_float_value_object():
    assert value_object("angle", "Float", 45.0) == {
        "label": "angle", "type": 6, "type_string": "Float", "value": 45.0
    }


def test_wavenumber_value_object():
    vo = value_object("kw", "Wavenumber", [4, 4])
    assert vo["type"] == 17 and vo["type_string"] == "Wavenumber"
    assert vo["value"] == [4, 4]


def test_value_range_adds_is_active():
    vo = value_object("post_remap", "Value range", [0.0, 1.0])
    assert vo["is_active"] is True and vo["value"] == [0.0, 1.0]


def test_dict_value_passthrough_for_advanced_type():
    full = {"label": "gradient", "type": 3, "type_string": "Color gradient", "value": []}
    assert value_object("gradient", "Color gradient", full) == full


def test_unknown_scalar_type_requires_full_object():
    try:
        value_object("g", "Color gradient", 5)
        assert False, "expected ParamError"
    except ParamError as e:
        assert "full value object" in str(e).lower()


# --- enum / choice resolution tests ---

@pytest.fixture(scope="module")
def ec():
    return EnumCatalog.load()


def test_enumeration_string_resolves_to_value_object(ec):
    vo = value_object("blending_method", "Enumeration", "maximum",
                      node_type="Blend", enum_catalog=ec)
    assert vo == {
        "label": "blending_method",
        "type": 4,
        "type_string": "Enumeration",
        "choice": "maximum",
        "value": 3,
    }


def test_enumeration_invalid_choice_raises_enum_error(ec):
    with pytest.raises(EnumError):
        value_object("blending_method", "Enumeration", "not_a_method",
                     node_type="Blend", enum_catalog=ec)


def test_enumeration_uncatalogued_param_raises_param_error(ec):
    # "NoiseFbm" has no Enumeration params → resolve returns None → ParamError
    with pytest.raises(ParamError, match="auto-resolvable"):
        value_object("kw", "Enumeration", "something",
                     node_type="NoiseFbm", enum_catalog=ec)


def test_enumeration_no_catalog_raises_param_error():
    # Without enum_catalog, string Enumeration must raise ParamError
    with pytest.raises(ParamError):
        value_object("blending_method", "Enumeration", "maximum")


def test_choice_string_returns_type1_object():
    vo = value_object("some_choice", "Choice", "optionA")
    assert vo == {
        "label": "some_choice",
        "type": 1,
        "type_string": "Choice",
        "value": "optionA",
    }


def test_dict_passthrough_still_works_for_enumeration(ec):
    full = {"label": "blending_method", "type": 4, "type_string": "Enumeration",
            "choice": "maximum", "value": 3}
    assert value_object("blending_method", "Enumeration", full,
                        node_type="Blend", enum_catalog=ec) == full
