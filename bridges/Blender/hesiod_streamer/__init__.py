bl_info = {
    "name": "Hesiod Heightmap Streamer",
    "author": "Hesiod",
    "version": (0, 6),
    "blender": (4, 2, 0),
    "category": "Object",
}

import bpy

from .mesh import restore_terrain_state
from .operators import (
    HESIOD_OT_arrange_selection,
    HESIOD_OT_normalize_domain,
    HESIOD_OT_start_stream,
)
from .ui import HESIOD_PT_panel, refresh_ui


# --- Registration

classes = (
    HESIOD_OT_start_stream,
    HESIOD_OT_arrange_selection,
    HESIOD_OT_normalize_domain,
    HESIOD_PT_panel,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)

    bpy.types.Scene.hesiod_port = bpy.props.IntProperty(
        name="Port",
        default=9001,
        min=1024,
        max=65535,
    )
    bpy.types.Scene.hesiod_accumulate = bpy.props.BoolProperty(
        name="Accumulate Meshes",
        description="Keep old generated meshes hidden instead of deleting them when terrain updates",
        default=False,
    )

    if not bpy.app.timers.is_registered(refresh_ui):
        bpy.app.timers.register(refresh_ui,
                                first_interval=1.0,
                                persistent=True)

    # defer until bpy.data is fully accessible
    bpy.app.timers.register(restore_terrain_state, first_interval=0.1)


def unregister():
    if bpy.app.timers.is_registered(refresh_ui):
        bpy.app.timers.unregister(refresh_ui)

    del bpy.types.Scene.hesiod_port
    del bpy.types.Scene.hesiod_accumulate

    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)


if __name__ == "__main__":
    register()