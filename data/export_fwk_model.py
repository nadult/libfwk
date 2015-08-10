#Written for Blender 2.75
#Author: Krzysztof 'nadult' Jakubowski

#TODO: if we have 2 armatures, then they can have bones with same names
#      identify nodes better than by just using their names

import re
import bpy
import mathutils
import xml.etree.ElementTree as ET

Quaternion = mathutils.Quaternion
Vector = mathutils.Vector

g_logging = True

def log(message):
    if g_logging:
        print(message)

def relativeDifference(a, b):
    magnitude = max(abs(a), abs(b))
    return 0.0 if (magnitude < 0.000001) else (abs(a - b) / magnitude)

def isValidString(string):
    return string.split() == [string]

def fixMatrixUpAxis(mat):
    mat = mat.copy()
    i = 0
    while i < 4:
        mat[1][i], mat[2][i] = mat[2][i], mat[1][i]
        mat[2][i] *= -1.0
        i = i + 1
    mat.transpose()
    i = 0
    while i < 4:
        mat[1][i], mat[2][i] = mat[2][i], mat[1][i]
        mat[2][i] *= -1.0
        i = i + 1
    mat.transpose()
    return mat

def fixVectorUpAxis(vec):
    return Vector((vec.x, vec.z, -vec.y))

def formatFloat(f):
    if relativeDifference(float(int(round(f))), f) < 0.00001:
        return str(int(round(f)))
    else:
        out = "%f" % f
        while len(out) > 1 and out[-1] == '0':
            out = out[:-1]
        return out

def formatFloats(floats):
    out = ""
    for f in floats:
        if len(out) > 0:
            out += ' '
        out += formatFloat(f)
    return out

def vecToString(vec):
    return formatFloats([vec.x, vec.y, vec.z])

def colorToString(col):
    return formatFloats([col[0], col[1], col[2]])

def quatToString(quat):
    return formatFloats([quat.x, quat.y, quat.z, quat.w])

def xmlIndent(elem, level=0):
  i = "\n" + level*"\t"
  if len(elem):
    if not elem.text or not elem.text.strip():
      elem.text = i + "\t"
    if not elem.tail or not elem.tail.strip():
      elem.tail = i
    for elem in elem:
      xmlIndent(elem, level+1)
    if not elem.tail or not elem.tail.strip():
      elem.tail = i
  else:
    if level and (not elem.tail or not elem.tail.strip()):
      elem.tail = i

def prepareMesh(obj):
    for mod in obj.modifiers:
        if mod.type == "ARMATURE":
            mod.show_viewport = False
    return obj.to_mesh(bpy.context.scene, apply_modifiers=True, settings="PREVIEW")

def writeSkin(xml_parent, mesh, obj):
    num_verts = len(mesh.vertices)
    log ("Skin for: " + obj.name)

    counts = []
    weights = []
    node_ids = []
    node_names = []
    group_index = [-1] * len(obj.vertex_groups)
    do_export = [0] * len(obj.vertex_groups)

    armature = obj.find_armature()
    if not armature.is_visible(bpy.context.scene):
        return;

    for vg in obj.vertex_groups:
        if(not isValidString(vg.name)):
            raise Exception("Vertex groups names mustn't contain whitespaces: \"" + obj.name + '"')
        do_export[vg.index] = armature.data.bones.find(vg.name) != -1
        if do_export[vg.index]:
            group_index[vg.index] = len(node_names)
            node_names.append(vg.name)
        else:
            group_index[vg.index] = -1
            log("Skipping vertex group: " + vg.name)

    for vertex in mesh.vertices:
        vweights = []
        vnode_ids = []
        for group in vertex.groups:
            index = group_index[group.group]
            if group.weight > 0.000001 and index != -1:
                vweights.append(group.weight)
                vnode_ids.append(index)

        weight_sum = sum(vweights)
        if weight_sum > 0.000001:
            weights.extend(formatFloat(w / weight_sum) for w in vweights)
            node_ids.extend(str(j) for j in vnode_ids)
            counts.append(str(len(vweights)))
        else:
            counts.append("0")

    xml_skin = xml_parent
    (ET.SubElement(xml_skin, "vertex_weight_counts")).text = " ".join(counts)
    (ET.SubElement(xml_skin, "vertex_weights")).text = " ".join(weights)
    (ET.SubElement(xml_skin, "vertex_weight_node_ids")).text = " ".join(node_ids)
    (ET.SubElement(xml_skin, "node_names")).text = " ".join(node_names)

def writeMesh(xml_parent, mesh, obj):
    xml_mesh_node = ET.SubElement(xml_parent, "mesh")
#   xml_mesh_node.attrib["name"] = mesh.name
    mesh_positions = []
    index_sets = []
    materials = []
    for mat in mesh.materials:
        index_sets.append([])
    if not mesh.materials:
        index_sets = [[]]

    if not mesh.tessfaces:
        mesh.calc_tessface()
    for face in mesh.tessfaces:
        verts = face.vertices
        tris = []
        if len(verts) == 4:
            tris = [verts[0], verts[1], verts[2], verts[2], verts[3], verts[0]]
        else:
            tris = [verts[0], verts[1], verts[2]]
        index_sets[face.material_index].extend(tris)

    for vertex in mesh.vertices:
        mesh_positions.append(vecToString(fixVectorUpAxis(vertex.co)))
    xml_positions = ET.SubElement(xml_mesh_node, "positions")
    xml_positions.text = ' '.join(mesh_positions)

    mat_idx = 0
    while mat_idx < len(index_sets):
        xml_layer = ET.SubElement(xml_mesh_node, "indices")
        if mesh.materials:
            materials.append(mesh.materials[mat_idx].name)
        xml_layer.text = ' '.join(map(str, index_sets[mat_idx]))
        mat_idx += 1

    if materials:
        ET.SubElement(xml_mesh_node, "materials").text =  ' '.join(materials)

    if obj.find_armature() and obj.find_armature().is_visible(bpy.context.scene):
        writeSkin(xml_mesh_node, mesh, obj)

def genTrans(matrix):
    pos, rot, scale = fixMatrixUpAxis(matrix).decompose()
    return (vecToString(pos), quatToString(rot), vecToString(scale))


def writeTrans(xml_node, matrix):
    (pos, rot, scale) = genTrans(matrix)
    if pos != "0 0 0":
        xml_node.set("pos", pos)
    if rot != "0 0 0 1":
        xml_node.set("rot", rot)
    if scale != "1 1 1":
        xml_node.set("scale", scale)

def writeBone(xml_parent, bone, bone_map):
    if(not isValidString(bone.name)):
        raise Exception("Bone names mustn't contain whitespaces: \"" + obj.name + '"')

    xml_bone = ET.SubElement(xml_parent, "node")
    xml_bone.set("name", bone.name)
    xml_bone.set("type", "generic")

    matrix = bone.matrix_local.copy()
    if not (bone.parent is None):
        parent_inv = bone.parent.matrix_local.copy()
        parent_inv.invert()
        matrix = parent_inv * matrix
    
    bone_map[bone.name] = (xml_bone, genTrans(matrix))
    writeTrans(xml_bone, matrix)

    for child in bone.children:
        writeBone(xml_bone, child, bone_map)

def objectTypeToString(obj):
#    if obj.type == "MESH":
#        return "mesh"
#    if obj.type == "ARMATURE":
#        return "armature"
#    if obj.type == "BONE":
#        return "bone"
    if obj.type == "EMPTY":
        return "empty"

    return "generic"

def writeObject(xml_parent, obj, mesh_list, bone_map):
    if(not isValidString(obj.name)):
        raise Exception("Object names mustn't contain whitespaces: \"" + obj.name + '"')

    matrix = obj.matrix_local.copy()
    if obj.parent:
        if(obj.parent_bone and obj.parent_type == "BONE"):
            xml_parent = bone_map[obj.parent_bone][0]
            bone = obj.parent.pose.bones[obj.parent_bone]
            bone_world = obj.parent.matrix_world * bone.matrix
            obj_world = obj.matrix_world
            matrix = bone_world.inverted() * obj_world
        elif obj.parent_type != "OBJECT":
            raise Exception("Unsupported parenting type: " + obj.parent_type)

    xml_obj_node = ET.SubElement(xml_parent, "node")
    xml_obj_node.set("name", obj.name)
    obj_type = objectTypeToString(obj)
    if obj_type != "generic":
        xml_obj_node.set("type", obj_type)

    if obj.type == "EMPTY":
        keys = list(set(obj.keys()) - set(["_RNA_UI", "cycles_visibility", "cycles"]))
        for key in keys:
            xml_prop = ET.SubElement(xml_obj_node, "property")
            xml_prop.set("name", key)
            xml_prop.set("value", str(obj[key]))

    writeTrans(xml_obj_node, matrix)

    if obj.type == "MESH":
        xml_obj_node.set("mesh_id", str(len(mesh_list)))
        mesh = prepareMesh(obj)
        mesh_list.append((mesh, obj))
    if obj.type == "ARMATURE":
        armature = obj.data
        armature.pose_position = "REST"
        for bone in armature.bones:
            if bone.parent is None:
                writeBone(xml_obj_node, bone, bone_map)
    
    scene = bpy.context.scene
    for child in obj.children:
        if child.is_visible(scene):
            writeObject(xml_obj_node, child, mesh_list, bone_map)

def extractMarkers(action):
    markers = set([])
    for curve in action.fcurves:
        cmarkers = []
        for kf in curve.keyframe_points:
            cmarkers.append(kf.co[0])
        markers = markers | set(cmarkers)
    log("Markers: " + str(sorted(list(markers))))
    return sorted(list(markers))

def writeTimeTrack(xml_parent, track, frame_range, fps, is_shared):
    xml_track = ET.SubElement(xml_parent, "shared_time_track" if is_shared else "time")
    text = ""
    for key in track:
        if text:
            text += " "
        text += formatFloat((key - frame_range[0]) / fps)
    xml_track.text = text


def writeAnim(xml_parent, action, armature, bone_map):
    armature_data = armature.data
    armature_data.pose_position = "POSE"

    for layer in armature_data.layers:
        layer = True
    for bone in armature_data.bones:
        bone.hide = False

    log("Action: " + action.name + " type: " + action.id_root) 
    if not armature.animation_data:
        armature.animation_data_create()
    armature.animation_data.action = action

    fps = bpy.context.scene.render.fps
    markers = extractMarkers(action)

    if (not markers) or (not armature.is_visible(bpy.context.scene)):
        return
    
    xml_anim = ET.SubElement(xml_parent, "anim")
    xml_anim.set("name", action.name)
    xml_anim.set("length", formatFloat((action.frame_range[1] - action.frame_range[0] + 1) / fps))

    matrices_2d = []

    bpy.context.scene.frame_set(0)
    for marker in markers:
        bpy.context.scene.frame_set(marker)
        bpy.context.scene.update()
        log("Frame: " + str(marker))

        matrices = []
        index = 0
        for bone in armature.pose.bones:
            matrix = bone.matrix.copy()
            if bone.parent:
                parent_inv = bone.parent.matrix.copy()
                parent_inv.invert()
                matrix = parent_inv * matrix
            matrices.append(fixMatrixUpAxis(matrix))
        matrices_2d.append(matrices)

    index = 0
    for bone in armature.pose.bones:
        xml_channel = ET.SubElement(xml_anim, "channel")
        xml_channel.set("name", bone.name)
        (def_pos, def_rot, def_scale) = bone_map[bone.name][1]

        positions = []
        rotations = []
        scales = []
        anything_added = False

        marker_idx = 0
        while marker_idx < len(markers):
            matrix = matrices_2d[marker_idx][index]
            pos, rot, scale = matrix.decompose()
            positions.append(vecToString(pos))
            rotations.append(quatToString(rot))
            scales.append(vecToString(scale))
            marker_idx = marker_idx + 1

        if any(pos != def_pos for pos in positions):
            if all(pos == positions[0] for pos in positions):
                xml_channel.set("pos", positions[0])
            else:
                xml_positions = ET.SubElement(xml_channel, "pos")
                xml_positions.text = " ".join(positions)
            anything_added = True
        if any(rot != def_rot for rot in rotations):
            if all(rot == rotations[0] for rot in rotations):
                xml_channel.set("rot", rotations[0])
            else:
                xml_rotations = ET.SubElement(xml_channel, "rot")
                xml_rotations.text = " ".join(rotations)
            anything_added = True
        if any(scale != def_scale for scale in scales):
            if all(scale == scales[0] for scale in scales):
                xml_channel.set("scale", scales[0])
            else:
                xml_scales = ET.SubElement(xml_channel, "scale")
                xml_scales.text = " ".join(scales)
            anything_added = True

        if not anything_added:
            xml_anim.remove(xml_channel)
        index = index + 1
    
    writeTimeTrack(xml_anim, markers, action.frame_range, fps, True)

def materialDiffuse(mat):
    if mat.use_nodes:
        col = mat.node_tree.nodes['Diffuse BSDF'].inputs[0].default_value
        return col
    else:
        return mat.diffuse_color

def writeMaterial(xml_parent, mat):
    if(not isValidString(mat.name)):
        raise Exception("Material names mustn't contain whitespaces: \"" + obj.name + '"')

    xml_mat = ET.SubElement(xml_parent, "material")
    xml_mat.set("name", mat.name)
    xml_mat.set("diffuse", colorToString(materialDiffuse(mat)))

# objects_filter: a regular expression for object names, like "human.*"
def write(file_path, objects_filter=""):
    context = bpy.context
    scene = context.scene

    if objects_filter:
        for object in scene.objects:
            object.hide = not re.search(objects_filter, object.name)
            print("Hide " + object.name + ": " + str(object.hide))
    else:
        for object in scene.objects:
            object.hide = False
    scene.layers = [True] * len(scene.layers)

    xml_root = ET.Element("model")

    mesh_list = []
    armatures = []
    bone_map = {}

    for obj in scene.objects:
        if (obj.parent is None) and (obj.is_visible(scene)):
            writeObject(xml_root, obj, mesh_list, bone_map)
            if obj.type == "ARMATURE":
                armatures.append(obj)

    if len(armatures) > 1:
        raise Exception("Exporting multiple armatures is not supported")
    armature = armatures[0] if (len(armatures) > 0) else None

    for (mesh, obj) in mesh_list:
        writeMesh(xml_root, mesh, obj)

    for mat in bpy.data.materials:
        writeMaterial(xml_root, mat)
  
    if armature:
        for action in bpy.data.actions:
            writeAnim(xml_root, action, armature, bone_map)

    xmlIndent(xml_root)
    xml_tree = ET.ElementTree(xml_root)
    xml_tree.write(file_path)

