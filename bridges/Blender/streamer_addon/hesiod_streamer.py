bl_info = {
    "name": "Hesiod Heightmap Streamer",
    "author": "Hesiod",
    "version": (1, 2),
    "blender": (5, 1, 2),
    "category": "Object",
}

import bpy
import socket
import struct
import zlib
import numpy as np
import threading

HOST = "127.0.0.1"
PLANE_NAME = "HeightPlane"
IMAGE_NAME = "HesiodTexture"
MATERIAL_NAME = "HesiodMat"

_thread = None
_vertex_buffer = None
_mesh_width = None
_mesh_height = None

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
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    try:
        sock.connect((HOST, port))
        print(f"[Hesiod] Connected on port {port}")

        while True:

            # ------------------------
            # HEADER
            # ------------------------

            header = recv_all(sock, 16)
            if not header:
                break

            width, height, hm_size, has_texture = struct.unpack("IIII", header)

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
                lambda hm=heightmap, c=rgba: update_mesh(hm, c))

    except Exception as e:
        print("[Hesiod] Socket error:", e)

    finally:
        sock.close()
        print("[Hesiod] Disconnected")


# ============================================================
# MATERIAL / TEXTURE
# ============================================================


def ensure_material(obj, width, height):
    mat = bpy.data.materials.get(MATERIAL_NAME)

    if mat is None:
        mat = bpy.data.materials.new(name=MATERIAL_NAME)
        mat.use_nodes = True

        nodes = mat.node_tree.nodes
        links = mat.node_tree.links
        nodes.clear()

        output = nodes.new("ShaderNodeOutputMaterial")
        bsdf = nodes.new("ShaderNodeBsdfPrincipled")
        tex = nodes.new("ShaderNodeTexImage")
        tex.name = "HesiodTexNode"

        links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
        links.new(bsdf.outputs["BSDF"], output.inputs["Surface"])

    if obj.data.materials:
        obj.data.materials[0] = mat
    else:
        obj.data.materials.append(mat)

    img = bpy.data.images.get(IMAGE_NAME)
    if img is None or img.size[0] != width or img.size[1] != height:
        if img is not None:
            bpy.data.images.remove(img)
        img = bpy.data.images.new(IMAGE_NAME,
                                  width=width,
                                  height=height,
                                  alpha=True)
        img.colorspace_settings.name = 'sRGB'

    mat.node_tree.nodes["HesiodTexNode"].image = img

    return img


def update_texture(img, rgba):
    flat = rgba.reshape(-1)
    img.pixels.foreach_set(flat)
    img.update()


# ============================================================
# MESH
# ============================================================


def create_grid(width, height):
    global _vertex_buffer, _mesh_width, _mesh_height

    obj = bpy.data.objects.get(PLANE_NAME)
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
    obj.name = PLANE_NAME

    obj.scale.x = aspect
    obj.scale.y = 1.0
    obj.scale.z = 1.0

    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)

    mesh = obj.data
    _vertex_buffer = np.empty(len(mesh.vertices) * 3, dtype=np.float32)
    mesh.vertices.foreach_get("co", _vertex_buffer)

    _mesh_width = width
    _mesh_height = height

    print(
        f"[Hesiod] Created grid {width}x{height} ({len(mesh.vertices)} vertices)"
    )

    return obj


def update_mesh(heightmap, rgba=None):
    global _vertex_buffer, _mesh_width, _mesh_height

    height, width = heightmap.shape
    aspect = width / height

    obj = bpy.data.objects.get(PLANE_NAME)
    if obj is None or _mesh_width != width or _mesh_height != height:
        obj = create_grid(width, height)

    # Reapply aspect scale on every update
    obj.scale.x = aspect
    obj.scale.y = 1.0
    obj.scale.z = 1.0

    mesh = obj.data
    z_scale = get_z_scale()
    heights = heightmap.flatten().astype(np.float32) * z_scale

    expected_vertices = len(mesh.vertices)
    if len(heights) != expected_vertices:
        print("[Hesiod] Vertex mismatch:", len(heights), expected_vertices)
        return None

    _vertex_buffer[2::3] = heights
    mesh.vertices.foreach_set("co", _vertex_buffer)
    mesh.update()

    if rgba is not None:
        img = ensure_material(obj, width, height)
        update_texture(img, rgba)

    return None


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
# UI OPERATOR
# ============================================================


class HESIOD_OT_start_stream(bpy.types.Operator):
    bl_idname = "hesiod.start_stream"
    bl_label = "Start Heightmap Stream"

    def execute(self, context):
        port = context.scene.hesiod_port
        start_stream(port)
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
        layout.prop(scene, "hesiod_port")
        layout.prop(scene, "hesiod_z_scale")
        layout.operator("hesiod.start_stream")


# ============================================================
# REGISTER
# ============================================================

classes = (
    HESIOD_OT_start_stream,
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


def unregister():
    del bpy.types.Scene.hesiod_z_scale
    del bpy.types.Scene.hesiod_port

    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
