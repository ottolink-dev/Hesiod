import time
import numpy as np
import bpy
from mathutils import Vector

from .constants import AUTOARRANGE_DEBOUNCE, Z_SCALE
from .materials import ensure_material, update_texture
from . import state
from .utils import (
    TILE_NAME_PATTERN,
    get_accumulate,
    get_autoarrange,
    get_normalize_scale,
    lowest_free_number,
    plane_name,
)


# --- Mesh scaling and auto-arrange


def normalize_domain_scale(obj):
    # unhide temporarily to evaluate dimensions cleanly
    was_hidden = obj.hide_viewport
    obj.hide_viewport = False

    bbox = [Vector(corner) for corner in obj.bound_box]
    orig_dim_x = max(pt.x for pt in bbox) - min(pt.x for pt in bbox)
    orig_dim_y = max(pt.y for pt in bbox) - min(pt.y for pt in bbox)

    if orig_dim_x > 0:
        obj.scale.x *= (1.0 / orig_dim_x)
    if orig_dim_y > 0:
        obj.scale.y *= (1.0 / orig_dim_y)

    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    obj.hide_viewport = was_hidden


def _run_autoarrange():
    state.autoarrange_pending = False

    tiles = [o for o in bpy.data.objects if "TerrainTile" in o.name]
    if not tiles:
        return None

    for o in bpy.context.selected_objects:
        o.select_set(False)
    for o in tiles:
        o.select_set(True)
    bpy.context.view_layer.objects.active = tiles[0]

    bpy.ops.hesiod.arrange_selection()
    return None


def schedule_autoarrange():
    if state.autoarrange_pending:
        return
    state.autoarrange_pending = True
    bpy.app.timers.register(_run_autoarrange,
                            first_interval=AUTOARRANGE_DEBOUNCE)


# --- Grid and mesh management


def create_grid(tid: int, width: int, height: int):
    prefix = plane_name(tid)
    accumulate = get_accumulate()

    prev_state = state.terrain_state.get(tid)
    prev_name = prev_state.get("live_name") if prev_state else None
    prev_obj = bpy.data.objects.get(prev_name) if prev_name else None

    freed_numbers = set()
    if prev_obj is not None and not accumulate:
        prev_match = TILE_NAME_PATTERN.match(prev_obj.name.split(".")[0])
        if prev_match:
            freed_numbers.add(int(prev_match.group(2)))
        state.terrain_state.setdefault(tid, {})["last_replaced_at"] = time.time()
        bpy.data.objects.remove(prev_obj, do_unlink=True)
    elif prev_obj is not None and accumulate:
        if not get_autoarrange():
            prev_obj.hide_set(True)

    order = lowest_free_number(tid, exclude=freed_numbers)
    name = f"{prefix}_{order}"

    aspect = width / height

    bpy.ops.mesh.primitive_grid_add(
        x_subdivisions=width - 1,
        y_subdivisions=height - 1,
        size=1.0,
        calc_uvs=True,
        enter_editmode=False,
        align='WORLD',
        location=(0.0, 0.0, 0.0),
        rotation=(0.0, 0.0, 0.0),
    )

    obj = bpy.context.active_object
    obj.name = name
    obj["hesiod_width"] = width
    obj["hesiod_height"] = height
    obj["hesiod_tid"] = tid

    obj.scale.x = aspect
    obj.scale.y = 1.0
    obj.scale.z = 1.0

    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)

    if get_normalize_scale():
        normalize_domain_scale(obj)

    mesh = obj.data
    vertex_buffer = np.empty(len(mesh.vertices) * 3, dtype=np.float32)
    mesh.vertices.foreach_get("co", vertex_buffer)

    st = state.terrain_state.setdefault(tid, {})
    st.update({
        "vertex_buffer": vertex_buffer,
        "mesh_width": width,
        "mesh_height": height,
        "live_name": obj.name,
    })

    print(f"[Hesiod] Created grid for terrain {tid}: "
          f"{width}x{height} ({len(mesh.vertices)} vertices)")

    if get_autoarrange():
        schedule_autoarrange()

    return obj


def update_mesh(tid: int, heightmap, rgba=None):
    height, width = heightmap.shape

    st = state.terrain_state.get(tid)
    live_name = st.get("live_name") if st else None
    obj = bpy.data.objects.get(live_name) if live_name else None

    needs_new_grid = (obj is None or st is None
                      or st["mesh_width"] != width
                      or st["mesh_height"] != height
                      or get_accumulate())

    if needs_new_grid:
        obj = create_grid(tid, width, height)
        st = state.terrain_state[tid]

    mesh = obj.data
    heights = heightmap.flatten().astype(np.float32) * Z_SCALE

    expected_vertices = len(mesh.vertices)
    if len(heights) != expected_vertices:
        print(f"[Hesiod] Terrain {tid} vertex mismatch: "
              f"{len(heights)} vs {expected_vertices}")
        return None

    st["vertex_buffer"][2::3] = heights
    mesh.vertices.foreach_set("co", st["vertex_buffer"])
    mesh.update()

    if rgba is not None:
        img = ensure_material(tid, obj, width, height)
        update_texture(img, rgba)

    return None


# --- Session restore


def restore_terrain_state():
    # rebuild terrain_state from objects already in the scene
    by_tid: dict[int, list] = {}
    for obj in bpy.data.objects:
        m = TILE_NAME_PATTERN.match(obj.name.split(".")[0])
        if m is None:
            continue
        by_tid.setdefault(int(m.group(1)), []).append(obj)

    for tid, objs in by_tid.items():
        visible = [o for o in objs if not o.hide_get()]
        obj = visible[0] if visible else None
        if obj is None:
            continue

        width = obj.get("hesiod_width")
        height = obj.get("hesiod_height")

        if width is None or height is None:
            print(
                f"[Hesiod] Terrain {tid} missing custom properties, skipping")
            continue

        mesh = obj.data
        vertex_buffer = np.empty(len(mesh.vertices) * 3, dtype=np.float32)
        mesh.vertices.foreach_get("co", vertex_buffer)

        state.terrain_state[tid] = {
            "vertex_buffer": vertex_buffer,
            "mesh_width": int(width),
            "mesh_height": int(height),
            "live_name": obj.name,
        }

        print(f"[Hesiod] Restored terrain {tid}: {width}x{height}")

    return None

