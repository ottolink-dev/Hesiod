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
