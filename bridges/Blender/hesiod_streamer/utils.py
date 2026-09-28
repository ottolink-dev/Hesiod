import bpy

# --- Naming


def plane_name(tid: int) -> str:
    return f"HeightPlane_{tid}"


def image_name(tid: int) -> str:
    return f"HesiodTexture_{tid}"


def material_name(tid: int) -> str:
    return f"HesiodMat_{tid}"


def tex_node_name(tid: int) -> str:
    return f"HesiodTexNode_{tid}"


# --- Safe getters


def get_accumulate() -> bool:
    return bpy.context.scene.hesiod_accumulate
