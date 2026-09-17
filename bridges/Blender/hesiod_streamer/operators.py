import bpy
from mathutils import Vector

from .network import start_stream


# --- Operators


class HESIOD_OT_start_stream(bpy.types.Operator):
    bl_idname = "hesiod.start_stream"
    bl_label = "Start Heightmap Stream"

    def execute(self, context):
        start_stream(context.scene.hesiod_port)
        return {'FINISHED'}


class HESIOD_OT_arrange_selection(bpy.types.Operator):
    bl_idname = "hesiod.arrange_selection"
    bl_label = "Arrange Selection in Line"
    bl_description = "Arrange selected objects side-by-side on X axis touching edges (oldest furthest left)"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        selected = [obj for obj in context.selected_objects if obj.type == 'MESH']
        if not selected:
            self.report({'WARNING'}, "No mesh objects selected")
            return {'CANCELLED'}

        # sort selected objects by name (oldest history timestamps come first)
        selected.sort(key=lambda o: o.name)

        current_x = 0.0
        for i, obj in enumerate(selected):
            # ensure unhidden so bounding box calculations work
            obj.hide_viewport = False

            bbox = [obj.matrix_world @ Vector(corner) for corner in obj.bound_box]
            min_x = min(pt.x for pt in bbox)
            max_x = max(pt.x for pt in bbox)
            width_x = max_x - min_x

            if i == 0:
                current_x = min_x
            else:
                shift_x = current_x - min_x
                obj.location.x += shift_x

            current_x += width_x

        self.report({'INFO'}, f"Arranged {len(selected)} object(s)")
        return {'FINISHED'}


class HESIOD_OT_normalize_domain(bpy.types.Operator):
    bl_idname = "hesiod.normalize_domain"
    bl_label = "Normalize Domain Coordinates"
    bl_description = "Set selected objects to 1x1m on X and Y, and apply scale transform"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        selected = [obj for obj in context.selected_objects if obj.type == 'MESH']
        if not selected:
            self.report({'WARNING'}, "No mesh objects selected")
            return {'CANCELLED'}

        prev_active = context.view_layer.objects.active

        for obj in selected:
            context.view_layer.objects.active = obj

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

        context.view_layer.objects.active = prev_active
        self.report({'INFO'}, f"Normalized scale for {len(selected)} object(s)")
        return {'FINISHED'}
