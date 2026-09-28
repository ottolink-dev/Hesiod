import bpy
from mathutils import Vector

from .network import disconnect_stream, start_stream
from .utils import get_arrange_spacing, tile_sort_key


# --- Operators


class HESIOD_OT_start_stream(bpy.types.Operator):
    bl_idname = "hesiod.start_stream"
    bl_label = "Start Heightmap Stream"

    def execute(self, context):
        start_stream(context.scene.hesiod_port)
        return {'FINISHED'}


class HESIOD_OT_disconnect_stream(bpy.types.Operator):
    bl_idname = "hesiod.disconnect_stream"
    bl_label = "Disconnect"
    bl_description = "Close the current heightmap stream connection"

    def execute(self, context):
        disconnect_stream()
        self.report({'INFO'}, "Disconnected")
        return {'FINISHED'}


class HESIOD_OT_arrange_selection(bpy.types.Operator):
    bl_idname = "hesiod.arrange_selection"
    bl_label = "Arrange"
    bl_description = (
        "Reset rotation/position, then lay out selected tiles in a grid: "
        "each terrain gets its own row (alphabetical by id, along Y), with "
        "the highest-numbered (most recent) tile closest to X=0"
    )
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        selected = [obj for obj in context.selected_objects if obj.type == 'MESH']
        if not selected:
            self.report({'WARNING'}, "No mesh objects selected")
            return {'CANCELLED'}

        spacing = get_arrange_spacing()

        # reset rotation and position up front for every selected object
        for obj in selected:
            obj.hide_viewport = False
            obj.location = Vector((0.0, 0.0, 0.0))
            obj.rotation_euler = Vector((0.0, 0.0, 0.0))

        # group tiles by terrain id
        groups: dict[int, list] = {}
        for obj in selected:
            tid, order = tile_sort_key(obj)
            groups.setdefault(tid, []).append((order, obj))

        current_y = 0.0
        for tid in sorted(groups.keys(), key=lambda t: str(t)):
            # highest order number first
            entries = sorted(groups[tid], key=lambda e: e[0], reverse=True)

            row_depth_y = 0.0
            current_x = 0.0
            for order, obj in entries:
                dims = obj.dimensions
                width_x = dims.x
                depth_y = dims.y
                row_depth_y = max(row_depth_y, depth_y)

                obj.location.x = current_x
                current_x += width_x + spacing

            for order, obj in entries:
                obj.location.y = current_y

            current_y += row_depth_y + spacing

        self.report({'INFO'}, f"Arranged {len(selected)} object(s)")
        return {'FINISHED'}


class HESIOD_OT_set_scale(bpy.types.Operator):
    bl_idname = "hesiod.set_scale"
    bl_label = "Set Scale"
    bl_description = (
        "Reset rotation and uniformly scale selected tiles "
        "(same factor on all axes) so their height matches the target"
    )
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        selected = [obj for obj in context.selected_objects if obj.type == 'MESH']
        if not selected:
            self.report({'WARNING'}, "No mesh objects selected")
            return {'CANCELLED'}

        target_height = context.scene.hesiod_target_height
        skipped = 0

        for obj in selected:
            was_hidden = obj.hide_viewport
            obj.hide_viewport = False

            obj.rotation_euler = Vector((0.0, 0.0, 0.0))

            bbox = [Vector(corner) for corner in obj.bound_box]
            current_height = max(pt.z for pt in bbox) - min(pt.z for pt in bbox)

            if current_height > 0:
                factor = target_height / current_height
                obj.scale.x *= factor
                obj.scale.y *= factor
                obj.scale.z *= factor
            else:
                skipped += 1

            obj.hide_viewport = was_hidden

        msg = (
            f"Set height to {target_height:.3f} for "
            f"{len(selected) - skipped} object(s) - scale was not applied"
        )
        if skipped:
            msg += f" ({skipped} skipped, zero height)"
        self.report({'INFO'}, msg)
        return {'FINISHED'}
