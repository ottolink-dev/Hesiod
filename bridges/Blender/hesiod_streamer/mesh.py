import re
import time
import numpy as np
import bpy

from .constants import Z_SCALE
from .materials import ensure_material, update_texture
from . import state
from .utils import get_accumulate, plane_name


# --- Grid and mesh management


def create_grid(tid: int, width: int, height: int):
    name = plane_name(tid)

    obj = bpy.data.objects.get(name)
    if obj is not None:
        if get_accumulate():
            # rename existing mesh and hide it (regular hide, toggleable with Alt+H)
            timestamp = int(time.time() * 1000)
            obj.name = f"{name}_history_{timestamp}"
            obj.hide_set(True)
        else:
            bpy.data.objects.remove(obj, do_unlink=True)

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

    obj.scale.x = aspect
    obj.scale.y = 1.0
    obj.scale.z = 1.0

    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)

    mesh = obj.data
    vertex_buffer = np.empty(len(mesh.vertices) * 3, dtype=np.float32)
    mesh.vertices.foreach_get("co", vertex_buffer)

    state.terrain_state[tid] = {
        "vertex_buffer": vertex_buffer,
        "mesh_width": width,
        "mesh_height": height,
    }

    print(f"[Hesiod] Created grid for terrain {tid}: "
          f"{width}x{height} ({len(mesh.vertices)} vertices)")

    return obj


def update_mesh(tid: int, heightmap, rgba=None):
    height, width = heightmap.shape

    terrain_info = state.terrain_state.get(tid)
    obj = bpy.data.objects.get(plane_name(tid))

    needs_new_grid = (obj is None or terrain_info is None
                      or terrain_info["mesh_width"] != width
                      or terrain_info["mesh_height"] != height
                      or get_accumulate())

    if needs_new_grid:
        obj = create_grid(tid, width, height)
        terrain_info = state.terrain_state[tid]

    mesh = obj.data
    heights = heightmap.flatten().astype(np.float32) * Z_SCALE

    expected_vertices = len(mesh.vertices)
    if len(heights) != expected_vertices:
        print(f"[Hesiod] Terrain {tid} vertex mismatch: "
              f"{len(heights)} vs {expected_vertices}")
        return None

    terrain_info["vertex_buffer"][2::3] = heights
    mesh.vertices.foreach_set("co", terrain_info["vertex_buffer"])
    mesh.update()

    if rgba is not None:
        img = ensure_material(tid, obj, width, height)
        update_texture(img, rgba)

    return None


# --- Session restore


def restore_terrain_state():
    """Rebuild terrain_state from objects already in the scene (e.g. after restart)."""
    pattern = re.compile(r"^HeightPlane_(\d+)$")

    for obj in bpy.data.objects:
        m = pattern.match(obj.name)
        if m is None:
            continue

        tid = int(m.group(1))
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
        }

        print(f"[Hesiod] Restored terrain {tid}: {width}x{height}")

    return None
