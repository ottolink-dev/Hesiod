import bpy

from .utils import image_name, material_name, tex_node_name


# --- Material and texture management


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
