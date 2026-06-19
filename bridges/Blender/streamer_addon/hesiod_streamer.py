bl_info = {
    "name": "Hesiod Heightmap Streamer",
    "author": "Hesiod",
    "version": (1, 4),
    "blender": (5, 1, 2),
    "category": "Object",
}

import re
import bpy
import socket
import struct
import zlib
import numpy as np
import threading

HOST = "127.0.0.1"

_thread = None
_connected = False
_terrain_state: dict[int, dict] = {}

# ============================================================
# PER-TERRAIN NAMING
# ============================================================


def plane_name(tid: int) -> str:
    return f"HeightPlane_{tid}"


def image_name(tid: int) -> str:
    return f"HesiodTexture_{tid}"


def material_name(tid: int) -> str:
    return f"HesiodMat_{tid}"


def tex_node_name(tid: int) -> str:
    return f"HesiodTexNode_{tid}"


# ============================================================
# SAFE GETTERS
# ============================================================


def get_z_scale():
    return bpy.context.scene.hesiod_z_scale


# ============================================================
# NETWORKING
# ============================================================


def recv_all(sock, size):
    data = b""
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk:
            return None
        data += chunk
    return data


def stream_loop(port):
    global _connected

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    try:
        sock.connect((HOST, port))
        _connected = True
        print(f"[Hesiod] Connected on port {port}")

        while True:

            # ------------------------
            # HEADER
            # ------------------------

            header = recv_all(sock, 20)
            if not header:
                break

            tid, width, height, hm_size, has_texture = struct.unpack(
                "IIIII", header)

            print(f"[Hesiod] Received terrain id={tid} ({width}x{height})")

            # ------------------------
            # HEIGHTMAP
            # ------------------------

            compressed_hm = recv_all(sock, hm_size)
            if compressed_hm is None:
                break

            raw_hm = zlib.decompress(compressed_hm)
            heightmap = np.frombuffer(raw_hm, dtype=np.float32).reshape(
                (height, width))

            rgba = None

            # ------------------------
            # TEXTURE
            # ------------------------

            if has_texture:
                tex_size_bytes = recv_all(sock, 4)
                if tex_size_bytes is None:
                    break

                tex_size = struct.unpack("I", tex_size_bytes)[0]

                compressed_rgba = recv_all(sock, tex_size)
                if compressed_rgba is None:
                    break

                raw_rgba = zlib.decompress(compressed_rgba)
                rgba = np.frombuffer(raw_rgba, dtype=np.float32).reshape(
                    (height, width, 4))

            bpy.app.timers.register(
                lambda hm=heightmap, c=rgba, t=tid: update_mesh(t, hm, c))

    except Exception as e:
        print("[Hesiod] Socket error:", e)

    finally:
        _connected = False
        sock.close()
        print("[Hesiod] Disconnected")


# ============================================================
# MATERIAL / TEXTURE
# ============================================================


def ensure_material(tid: int, obj, width: int, height: int):
    mat_name = material_name(tid)
    mat = bpy.data.materials.get(mat_name)

    if mat is None:
        mat = bpy.data.materials.new(name=mat_name)
        mat.use_nodes = True

        nodes = mat.node_tree.nodes
        links = mat.node_tree.links
        nodes.clear()

        output = nodes.new("ShaderNodeOutputMaterial")
        bsdf = nodes.new("ShaderNodeBsdfPrincipled")
        tex = nodes.new("ShaderNodeTexImage")
        tex.name = tex_node_name(tid)

        links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
        links.new(bsdf.outputs["BSDF"], output.inputs["Surface"])

    if obj.data.materials:
        obj.data.materials[0] = mat
    else:
        obj.data.materials.append(mat)

    img_name = image_name(tid)
    img = bpy.data.images.get(img_name)
    if img is None or img.size[0] != width or img.size[1] != height:
        if img is not None:
            bpy.data.images.remove(img)
        img = bpy.data.images.new(img_name,
                                  width=width,
                                  height=height,
                                  alpha=True)
        img.colorspace_settings.name = 'sRGB'

    mat.node_tree.nodes[tex_node_name(tid)].image = img

    return img


def update_texture(img, rgba):
    flat = rgba.reshape(-1)
    img.pixels.foreach_set(flat)
    img.update()


# ============================================================
# MESH
# ============================================================


def create_grid(tid: int, width: int, height: int):
    name = plane_name(tid)

    obj = bpy.data.objects.get(name)
    if obj is not None:
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

    _terrain_state[tid] = {
        "vertex_buffer": vertex_buffer,
        "mesh_width": width,
        "mesh_height": height,
    }

    print(f"[Hesiod] Created grid for terrain {tid}: "
          f"{width}x{height} ({len(mesh.vertices)} vertices)")

    return obj


def update_mesh(tid: int, heightmap, rgba=None):
    height, width = heightmap.shape

    state = _terrain_state.get(tid)
    obj = bpy.data.objects.get(plane_name(tid))

    needs_new_grid = (obj is None or state is None
                      or state["mesh_width"] != width
                      or state["mesh_height"] != height)

    if needs_new_grid:
        obj = create_grid(tid, width, height)
        state = _terrain_state[tid]

    mesh = obj.data
    z_scale = get_z_scale()
    heights = heightmap.flatten().astype(np.float32) * z_scale

    expected_vertices = len(mesh.vertices)
    if len(heights) != expected_vertices:
        print(f"[Hesiod] Terrain {tid} vertex mismatch: "
              f"{len(heights)} vs {expected_vertices}")
        return None

    state["vertex_buffer"][2::3] = heights
    mesh.vertices.foreach_set("co", state["vertex_buffer"])
    mesh.update()

    if rgba is not None:
        img = ensure_material(tid, obj, width, height)
        update_texture(img, rgba)

    return None


# ============================================================
# SESSION RESTORE
# ============================================================


def restore_terrain_state():
    """Rebuild _terrain_state from objects already in the scene (e.g. after restart)."""
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

        _terrain_state[tid] = {
            "vertex_buffer": vertex_buffer,
            "mesh_width": int(width),
            "mesh_height": int(height),
        }

        print(f"[Hesiod] Restored terrain {tid}: {width}x{height}")

    return None  # don't reschedule


# ============================================================
# UI REFRESH TIMER
# ============================================================


def _refresh_ui() -> float:
    for screen in bpy.data.screens:
        for area in screen.areas:
            if area.type == 'VIEW_3D':
                area.tag_redraw()
    return 1.0


# ============================================================
# STREAM CONTROL
# ============================================================


def start_stream(port):
    global _thread

    if _thread and _thread.is_alive():
        print("[Hesiod] Already running")
        return

    _thread = threading.Thread(target=stream_loop, args=(port, ), daemon=True)
    _thread.start()
    print(f"[Hesiod] Stream started on port {port}")


# ============================================================
# OPERATORS
# ============================================================


class HESIOD_OT_start_stream(bpy.types.Operator):
    bl_idname = "hesiod.start_stream"
    bl_label = "Start Heightmap Stream"

    def execute(self, context):
        start_stream(context.scene.hesiod_port)
        return {'FINISHED'}


class HESIOD_OT_clear_terrains(bpy.types.Operator):
    bl_idname = "hesiod.clear_terrains"
    bl_label = "Clear All Terrains"
    bl_description = "Remove all HeightPlane objects, materials, and textures created by Hesiod"

    def execute(self, context):
        removed = 0
        for tid in list(_terrain_state.keys()):
            obj = bpy.data.objects.get(plane_name(tid))
            if obj is not None:
                bpy.data.objects.remove(obj, do_unlink=True)
                removed += 1

            img = bpy.data.images.get(image_name(tid))
            if img is not None:
                bpy.data.images.remove(img)

            mat = bpy.data.materials.get(material_name(tid))
            if mat is not None:
                bpy.data.materials.remove(mat)

            del _terrain_state[tid]

        self.report({'INFO'}, f"Removed {removed} terrain(s)")
        return {'FINISHED'}


# ============================================================
# UI PANEL
# ============================================================


class HESIOD_PT_panel(bpy.types.Panel):
    bl_label = "Hesiod Stream"
    bl_idname = "HESIOD_PT_panel"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Hesiod"

    def draw(self, context):
        layout = self.layout
        scene = context.scene

        # Connection indicator
        row = layout.row()
        if _connected:
            row.label(text="● Connected", icon='CHECKMARK')
        else:
            row.label(text="○ Disconnected", icon='X')

        layout.separator()
        layout.prop(scene, "hesiod_port")
        layout.prop(scene, "hesiod_z_scale")
        layout.operator("hesiod.start_stream",
                        text="Reconnect" if _connected else "Connect")

        layout.separator()
        layout.label(text=f"Active terrains: {len(_terrain_state)}")
        layout.operator("hesiod.clear_terrains", icon='TRASH')


# ============================================================
# REGISTER
# ============================================================

classes = (
    HESIOD_OT_start_stream,
    HESIOD_OT_clear_terrains,
    HESIOD_PT_panel,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)

    bpy.types.Scene.hesiod_z_scale = bpy.props.FloatProperty(name="Z Scale",
                                                             default=0.2,
                                                             min=0.0,
                                                             max=1.0)
    bpy.types.Scene.hesiod_port = bpy.props.IntProperty(name="Port",
                                                        default=9001,
                                                        min=1024,
                                                        max=65535)

    if not bpy.app.timers.is_registered(_refresh_ui):
        bpy.app.timers.register(_refresh_ui,
                                first_interval=1.0,
                                persistent=True)

    # Defer until bpy.data is fully accessible
    bpy.app.timers.register(restore_terrain_state, first_interval=0.1)


def unregister():
    if bpy.app.timers.is_registered(_refresh_ui):
        bpy.app.timers.unregister(_refresh_ui)

    del bpy.types.Scene.hesiod_z_scale
    del bpy.types.Scene.hesiod_port

    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
