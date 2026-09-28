import re
import bpy

# --- Naming


def plane_name(tid: int) -> str:
    return f"TerrainTile_{tid}"


def image_name(tid: int) -> str:
    return f"HesiodTexture_{tid}"


def material_name(tid: int) -> str:
    return f"HesiodMat_{tid}"


def tex_node_name(tid: int) -> str:
    return f"HesiodTexNode_{tid}"


# --- Safe getters


def get_accumulate() -> bool:
    return bpy.context.scene.hesiod_accumulate


def get_normalize_scale() -> bool:
    return bpy.context.scene.hesiod_normalize_scale


def get_autoarrange() -> bool:
    return bpy.context.scene.hesiod_autoarrange


def get_arrange_spacing() -> float:
    return bpy.context.scene.hesiod_arrange_spacing


# --- Order numbering

TILE_NAME_PATTERN = re.compile(r"^TerrainTile_(\d+)_(\d+)$")


def used_orders_for(tid: int) -> set:
    prefix = f"TerrainTile_{tid}_"
    used = set()
    for obj in bpy.data.objects:
        if not obj.name.startswith(prefix):
            continue
        # strip any Blender-added .001 style suffix before parsing the number
        base_name = obj.name.split(".")[0]
        m = TILE_NAME_PATTERN.match(base_name)
        if m and int(m.group(1)) == tid:
            used.add(int(m.group(2)))
    return used


def lowest_free_number(tid: int, exclude: set = frozenset()) -> int:
    used = used_orders_for(tid) - exclude
    n = 1
    while n in used:
        n += 1
    return n


def tile_sort_key(obj):
    m = TILE_NAME_PATTERN.match(obj.name.split(".")[0])
    if m is None:
        return (None, 0)
    return (int(m.group(1)), int(m.group(2)))
