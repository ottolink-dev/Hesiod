import bpy

from . import state


# --- UI refresh timer


def refresh_ui() -> float:
    for screen in bpy.data.screens:
        for area in screen.areas:
            if area.type == 'VIEW_3D':
                area.tag_redraw()
    return 1.0


# --- UI Panel


class HESIOD_PT_panel(bpy.types.Panel):
    bl_label = "Hesiod Stream"
    bl_idname = "HESIOD_PT_panel"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Hesiod"

    def draw(self, context):
        layout = self.layout
        scene = context.scene

        # connection indicator
        row = layout.row()
        if state.connected:
            row.label(text="● Connected", icon='CHECKMARK')
        else:
            row.label(text="○ Disconnected", icon='X')

        layout.separator()
        layout.prop(scene, "hesiod_port")
        layout.prop(scene, "hesiod_accumulate")

        layout.operator("hesiod.start_stream",
                        text="Reconnect" if state.connected else "Connect")

        layout.separator()
        layout.label(text="Tools:")
        layout.operator("hesiod.arrange_selection", icon='ALIGN_CENTER')
        layout.operator("hesiod.normalize_domain", icon='FULLSCREEN_ENTER')

        layout.separator()
        layout.label(text=f"Active terrains: {len(state.terrain_state)}")
