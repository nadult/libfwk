#Written for Blender 2.75
#Author: Krzysztof 'nadult' Jakubowski

import bpy
import mathutils
import xml.etree.ElementTree as ET

Quaternion = mathutils.Quaternion
Vector = mathutils.Vector

logging = True

def log(message):
    if logging:
        print(message)

def relativeDifference(a, b):
    magnitude = max(abs(a), abs(b))
    return 0.0 if (magnitude < 0.000001) else (abs(a - b) / magnitude)

def formatFloat(f):
    if relativeDifference(float(int(round(f))), f) < 0.000001:
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

def formatMatrix(matrix):
    out = ""
    for row in matrix:
        if len(out) > 0:
            out += " "
        out += formatFloats(row)
    return out

def vecToString(vec):
    return formatFloats([vec.x, vec.y, vec.z])

def quatToString(quat):
    return formatFloats([quat.x, quat.y, quat.z, quat.w])

def xmlIndent(elem, level=0):
  i = "\n" + level*"  "
  if len(elem):
    if not elem.text or not elem.text.strip():
      elem.text = i + "  "
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

def writeSkin(xml_parent, mesh, obj, node_id_map):
    num_verts = len(mesh.vertices)
    log ("Skin for: " + obj.name)

    index_map = [0] * len(obj.vertex_groups)
    for vg in obj.vertex_groups:
        joint_id = node_id_map[vg.name] if (vg.name in node_id_map) else -1
        index_map[vg.index] = joint_id

    counts = []
    weights = []
    joint_ids = []

    for vertex in mesh.vertices:
        vweights = []
        vjoint_ids = []
        for group in vertex.groups:
            joint_id = index_map[group.group]
            if joint_id != -1 and group.weight > 0.000001:
                vweights.append(group.weight)
                vjoint_ids.append(joint_id)

        weight_sum = sum(vweights)
        if weight_sum > 0.000001:
            weights.extend(formatFloat(w / weight_sum) for w in vweights)
            joint_ids.extend(str(j) for j in vjoint_ids)
            counts.append(str(len(vweights)))
        else:
            counts.append("0")

    xml_skin = ET.SubElement(xml_parent, "skin")
    (ET.SubElement(xml_skin, "counts")).text = " ".join(counts)
    (ET.SubElement(xml_skin, "weights")).text = " ".join(weights)
    (ET.SubElement(xml_skin, "joint_ids")).text = " ".join(joint_ids)

def writeMesh(xml_parent, mesh, obj, node_id_map):
    xml_mesh_node = ET.SubElement(xml_parent, "simple_mesh")
#   xml_mesh_node.attrib["name"] = mesh.name
    xml_mesh_node.attrib["primitive_type"] = "triangles"
    mesh_positions = []
    mesh_indices = []

    if not mesh.tessfaces:
        mesh.calc_tessface()
    for face in mesh.tessfaces:
        verts = face.vertices
        tris = []
        if len(verts) == 4:
            tris = [verts[0], verts[1], verts[2], verts[2], verts[3], verts[0]]
        else:
            tris = face.vertices
        mesh_indices.extend(tris)
    for vertex in mesh.vertices:
        mesh_positions.extend(vertex.co)

    xml_positions = ET.SubElement(xml_mesh_node, "positions")
    xml_indices = ET.SubElement(xml_mesh_node, "indices")
    xml_positions.text = ' '.join(map(formatFloat, mesh_positions))
    xml_indices.text = ' '.join(map(str, mesh_indices))

    if obj.find_armature():
        writeSkin(xml_mesh_node, mesh, obj, node_id_map)


def writeTrans(xml_node, matrix):
    loc, rot, scale = matrix.decompose()
    xml_node.set("pos", vecToString(loc))
    if rot != Quaternion((1, 0, 0, 0)):
        xml_node.set("rot", quatToString(rot))
    str_scale = vecToString(scale)
    if str_scale != "1 1 1":
        xml_node.set("scale", str_scale)

def writeBone(xml_parent, bone, node_id_map):
    xml_bone = ET.SubElement(xml_parent, "node")
    xml_bone.set("name", bone.name)
    xml_bone.set("type", "bone")
    node_id_map[bone.name] = len(node_id_map)

    matrix = bone.matrix_local.copy()
    if not (bone.parent is None):
        parent_mat = bone.parent.matrix_local.copy()
        parent_mat.invert()
        matrix = parent_mat * matrix
    writeTrans(xml_bone, matrix)

    for child in bone.children:
        writeBone(xml_bone, child, node_id_map)

def objectTypeString(type):
    if type == "MESH":
        return "mesh"
    if type == "ARMATURE":
        return "skeleton"
    if type == "BONE":
        return "bone"
    return "unknown"

def writeObject(xml_parent, obj, mesh_list, node_id_map):
    xml_obj_node = ET.SubElement(xml_parent, "node")
    xml_obj_node.set("name", obj.name)
    xml_obj_node.set("type", objectTypeString(obj.type))
    node_id_map[obj.name] = len(node_id_map)

    writeTrans(xml_obj_node, obj.matrix_world)

    if not obj.parent is None:
        xml_obj_node.set("parent", obj.parent.name)
    if obj.type == "MESH":
        xml_obj_node.set("mesh_ids", str(len(mesh_list)))
        mesh = prepareMesh(obj)
        mesh_list.append((mesh, obj))
    if obj.type == "ARMATURE":
        armature = obj.data
        armature.pose_position = "REST"
        for bone in armature.bones:
            if bone.parent is None:
                writeBone(xml_obj_node, bone, node_id_map)
    
    scene = bpy.context.scene    
    for child in obj.children:
        if child.is_visible(scene):
            writeObject(xml_obj_node, child, mesh_list, node_id_map)

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


def writeAnim(xml_parent, action, armature, node_id_map):
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
    if not markers:
        return
    
    xml_anim = ET.SubElement(xml_parent, "anim")
    xml_anim.set("name", action.name)
    xml_anim.set("length", formatFloat((action.frame_range[1] - action.frame_range[0] + 1) / fps))
    writeTimeTrack(xml_anim, markers, action.frame_range, fps, True)

    bone_ids = []
    for bone in armature.pose.bones:
        bone_ids.append(node_id_map[bone.name])

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
                parent_mat = bone.parent.matrix.copy()
                parent_mat.invert()
                matrix = parent_mat * matrix
            matrices.append(matrix)
        matrices_2d.append(matrices)

    index = 0
    for bone in armature.pose.bones:
        xml_channel = ET.SubElement(xml_anim, "channel")
        xml_channel.set("joint_name", bone.name)
        xml_channel.set("joint_id", str(bone_ids[index]))

        positions = []
        rotations = []
        scales = []

        marker_idx = 0
        while marker_idx < len(markers):
            matrix = matrices_2d[marker_idx][index]
            loc, rot, scale = matrix.decompose()
            positions.append(vecToString(loc))
            rotations.append(quatToString(rot))
            scales.append(vecToString(scale))
            marker_idx = marker_idx + 1

        if any(pos != "0 0 0" for pos in positions):
            if all(pos == positions[0] for pos in positions):
                xml_channel.set("pos", positions[0])
            else:
                xml_positions = ET.SubElement(xml_channel, "pos")
                xml_positions.text = " ".join(positions)
        if any(rot != "0 0 0 1" for rot in rotations):
            if all(rot == rotations[0] for rot in rotations):
                xml_channel.set("rot", rotations[0])
            else:
                xml_rotations = ET.SubElement(xml_channel, "rot")
                xml_rotations.text = " ".join(rotations)
        if any(scale != "1 1 1" for scale in scales):
            if all(scale == scales[0] for scale in scales):
                xml_channel.set("scale", scales[0])
            else:
                xml_scales = ET.SubElement(xml_channel, "scale")
                xml_scales.text = " ".join(scales)

        index = index + 1

def write(file_path, layer_ids):
    context = bpy.context
    scene = context.scene
    for object in context.scene.objects:
        object.hide = False

    if len(layer_ids) == 0:
        context.scene.layers = [True] * len(context.scene.layers)
    else:
        context.scene.layers = [False] * len(context.scene.layers)
        for id in layer_ids:
            context.scene.layers[id] = True

    xml_root = ET.Element("mesh")

    node_id_map = {}
    mesh_list = []
    armature = None

    for obj in scene.objects:
        if (obj.parent is None) and (obj.is_visible(scene)):
            writeObject(xml_root, obj, mesh_list, node_id_map)
        if obj.type == "ARMATURE":
            armature = obj
            log("Armature: " + armature.name)

    for (mesh, obj) in mesh_list:
        writeMesh(xml_root, mesh, obj, node_id_map)
  
    if armature:
        for action in bpy.data.actions:
            writeAnim(xml_root, action, armature, node_id_map)

    xmlIndent(xml_root)
    xml_tree = ET.ElementTree(xml_root)
    xml_tree.write(file_path)

