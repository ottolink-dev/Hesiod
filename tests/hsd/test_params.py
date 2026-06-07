import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "scripts"))

from hsd.params import value_object, ParamError


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
