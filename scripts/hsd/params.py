from hsd.catalog import TYPE_MAP


class ParamError(Exception):
    pass


def value_object(name, type_string, value):
    """Build a Hesiod param value object for an overridden parameter.

    If `value` is a dict, it is treated as a fully-formed value object and passed
    through (escape hatch for advanced types like Color gradient/Cloud/Path/
    Enumeration). Otherwise the param type must be in TYPE_MAP.
    """
    if isinstance(value, dict):
        return value

    if type_string not in TYPE_MAP:
        raise ParamError(
            f"param '{name}' has type '{type_string}' which needs a full value "
            "object; pass a dict value instead of a scalar"
        )

    code, ts = TYPE_MAP[type_string]
    vo = {"label": name, "type": code, "type_string": ts, "value": value}
    if type_string == "Value range":
        vo["is_active"] = True
    return vo
