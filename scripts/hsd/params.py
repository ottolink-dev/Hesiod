from hsd.catalog import TYPE_MAP


class ParamError(Exception):
    pass


def value_object(name, type_string, value, *, node_type=None, enum_catalog=None):
    """Build a Hesiod param value object for an overridden parameter.

    If `value` is a dict, it is treated as a fully-formed value object and passed
    through (escape hatch for advanced types like Color gradient/Cloud/Path/
    Enumeration). Otherwise the param type must be in TYPE_MAP, or be handled by
    the enum/choice fast-paths below.

    Optional keyword args:
        node_type    -- required when resolving Enumeration string values.
        enum_catalog -- an EnumCatalog instance; required for Enumeration strings.
    """
    if isinstance(value, dict):
        return value

    if type_string == "Enumeration" and isinstance(value, str):
        if enum_catalog is None or node_type is None:
            raise ParamError(
                f"param '{name}' on node '{node_type}' has type 'Enumeration'; "
                "pass node_type= and enum_catalog= to resolve a string choice, "
                "or pass a full value-object dict instead"
            )
        int_value = enum_catalog.resolve(node_type, name, value)
        if int_value is None:
            raise ParamError(
                f"param '{name}' on node '{node_type}' is not auto-resolvable "
                "(not in the enum catalog); pass a full value-object dict instead"
            )
        # EnumError (invalid choice) propagates directly from resolve()
        return {
            "label": name,
            "type": 4,
            "type_string": "Enumeration",
            "choice": value,
            "value": int_value,
        }

    if type_string == "Choice" and isinstance(value, str):
        return {"label": name, "type": 1, "type_string": "Choice", "value": value}

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
