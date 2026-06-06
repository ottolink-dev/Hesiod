import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "scripts"))

from hsd.catalog import Catalog, TYPE_MAP


def test_catalog_loads_default_path():
    cat = Catalog.load()
    assert cat.has_node("NoiseFbm")
    assert not cat.has_node("NoSuchNode")


def test_catalog_ports_have_datatypes():
    cat = Catalog.load()
    out = cat.port("NoiseFbm", "output")
    assert out["type"] == "output"
    assert out["data_type"] == "VirtualArray"


def test_catalog_params_expose_type_strings():
    cat = Catalog.load()
    params = cat.params("NoiseFbm")
    assert params["kw"]["type"] == "Wavenumber"


def test_type_map_has_confirmed_codes():
    assert TYPE_MAP["Float"] == (6, "Float")
    assert TYPE_MAP["Wavenumber"] == (17, "Wavenumber")


def test_unknown_node_raises_descriptive_keyerror():
    cat = Catalog.load()
    import pytest
    with pytest.raises(KeyError, match="unknown node type"):
        cat.params("NoSuchNode")


def test_none_parameters_returns_empty_dict():
    # some nodes have "parameters": null in the JSON; params() must return {}
    cat = Catalog.load()
    # find a node whose raw parameters are null, then assert params() == {}
    null_param_nodes = [t for t in cat.node_types()
                        if cat._data[t].get("parameters") is None]
    if null_param_nodes:
        assert cat.params(null_param_nodes[0]) == {}
