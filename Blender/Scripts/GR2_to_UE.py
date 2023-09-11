import bpy
import re
import sys
import os
import json
import math

def delete_dummy():
    """Delete an object named 'dummy' from the scene if it exists."""
    dummy = bpy.data.objects.get('dummy')
    if dummy:
        bpy.data.objects.remove(dummy, do_unlink=True)
        print("Dummy Deleted")

def convert_dummy_to_plane():
    """Convert an object named 'dummy' to a plane."""
    dummy = bpy.data.objects.get('dummy')
    if dummy:
        bpy.data.objects.remove(dummy, do_unlink=True)
        bpy.ops.mesh.primitive_plane_add(size=0.01, enter_editmode=False, align='WORLD', location=(0, -100000, 0))
        plane = bpy.data.objects['Plane']
        plane.name = 'dummy'
        print("Dummy Converted")

def bone_to_empty(armature, bone, parent_empty=None):
    """Converts an armature bone to an empty and parents it under a given parent_empty."""
    
    # Calculate the world-space position for the bone's head
    global_pos = armature.matrix_world @ bone.head_local
    bpy.ops.object.empty_add(location=global_pos)
    empty = bpy.context.active_object
    empty.name = bone.name
    
    # If there's a parent_empty, set it as the parent of the new empty
    if parent_empty:
        empty.parent = parent_empty
    else:
        empty.parent = armature

    # Match the rotation of the bone to the empty using the bone's matrix
    empty.matrix_world = armature.matrix_world @ bone.matrix_local

    # For each child bone, recurse
    for child_bone in bone.children:
        bone_to_empty(armature, child_bone, empty)
    return empty

def skeleton_to_empties(armature_object):
    """Converts the bones of an armature to empties, nested under a root empty."""
    # Ensure we're in object mode
    bpy.ops.object.mode_set(mode='OBJECT')

    # Select armature and apply transform to armature
    bpy.ops.object.select_all(action='DESELECT')
    armature_object.select_set(True)

    # Create a root empty to parent all the bone empties, name it after the armature
    bpy.ops.object.empty_add(location=armature_object.location)
    root_empty = bpy.context.active_object
    root_name = armature_object.name
    
    # For each root bone (bones without a parent), create an empty
    for bone in armature_object.data.bones:
        if not bone.parent:
            bone_to_empty(armature_object, bone, root_empty)
            
    bpy.data.objects.remove(armature_object, do_unlink=True)
    root_empty.name = root_name
    print("Skeleton Converted")
    return root_empty

def make_empty_as_child(parent_empty, child_empty):
    """Move the child_empty under the parent_empty while preserving world space coordinates."""
    # Store the world matrix of the child_empty
    world_matrix = child_empty.matrix_world.copy()
    
    # Parent the child_empty to the parent_empty
    child_empty.parent = parent_empty
    
    # Set the child's local location to preserve its world space location
    child_empty.matrix_world = world_matrix
    
def move_PL_to_pointlight(root_empty):
    if not root_empty:
        raise ValueError("Root empty not found!")
        return

    # Check if there's an empty called "pointlight" under the root
    pointlight_empty = None
    for child in root_empty.children:
        if child.name == "pointlight":
            pointlight_empty = child
            break

    # If not, create it
    if not pointlight_empty:
        bpy.ops.object.empty_add(location=root_empty.location)
        pointlight_empty = bpy.context.active_object
        pointlight_empty.name = "pointlight"
        pointlight_empty.parent = root_empty

    # Check for empties that start with the pattern "PL#-"
    pattern = re.compile(r'^PL\d+-')
    for obj in bpy.data.objects:
        if obj.type == 'EMPTY' and pattern.match(obj.name):
            print("Moving " + obj.name + " to pointlight")
            make_empty_as_child(pointlight_empty, obj)
            obj.name = "SOCKET_PL_" + obj.name[3:]

    # If no light under pointlight, delete pointlight
    if len(pointlight_empty.children) == 0:
        bpy.data.objects.remove(pointlight_empty, do_unlink=True)


def move_decallocator_to_decal(root_empty):
    if not root_empty:
        raise ValueError("Root empty not found!")
        return

    # Check if there's an empty called "decal" under the root
    decal_empty = None
    for child in root_empty.children:
        if child.name == "decal":
            decal_empty = child
            break

    # If not, create it
    if not decal_empty:
        bpy.ops.object.empty_add(location=root_empty.location)
        decal_empty = bpy.context.active_object
        decal_empty.name = "decal"
        decal_empty.parent = root_empty

    # Check for empties that start with the pattern "G2DecalLocator"
    pattern = re.compile(r'^G2DecalLocator')
    for obj in bpy.data.objects:
        if obj.type == 'EMPTY' and pattern.match(obj.name):
            print("Moving " + obj.name + " to decal")
            make_empty_as_child(decal_empty, obj)
            obj.name = "SOCKET_DECAL_" + obj.name

    # If no decal under decal, delete decal
    if len(decal_empty.children) == 0:
        bpy.data.objects.remove(decal_empty, do_unlink=True)

def move_grasslocator_to_grass(root_empty):
    if not root_empty:
        raise ValueError("Root empty not found!")
        return

    # Check if there's an empty called "grass" under the root
    grass_empty = None
    for child in root_empty.children:
        if child.name == "grass":
            grass_empty = child
            break

    # If not, create it
    if not grass_empty:
        bpy.ops.object.empty_add(location=root_empty.location)
        grass_empty = bpy.context.active_object
        grass_empty.name = "grass"
        grass_empty.parent = root_empty

    # Check for empties that start with the pattern "G2grassLocator"
    pattern = re.compile(r'^G2GrassLocator')
    for obj in bpy.data.objects:
        if obj.type == 'EMPTY' and pattern.match(obj.name):
            print("Moving " + obj.name + " to grass")
            make_empty_as_child(grass_empty, obj)
            obj.name = "SOCKET_GRASS_" + obj.name

    # If no grass under grass, delete grass
    if len(grass_empty.children) == 0:
        bpy.data.objects.remove(grass_empty, do_unlink=True)

def convert_ext_to_socket():
    pattern = re.compile(r'^ext_')
    for obj in bpy.data.objects:
        if obj.type == 'EMPTY' and pattern.match(obj.name):
            print("Converting " + obj.name + " to socket")
            obj.name = "SOCKET_EXT_" + obj.name[4:]

def convert_decal_to_socket(): # Not sure if this is needed
    pattern = re.compile(r'^decal_')
    for obj in bpy.data.objects:
        if obj.type == 'EMPTY' and pattern.match(obj.name):
            print("Converting " + obj.name + " to socket")
            obj.name = "SOCKET_DECAL_" + obj.name[6:]


def delete_low_model():
    # delete all contains "low_model"
    pattern = re.compile(r'low_model')
    for obj in bpy.data.objects:
        if pattern.match(obj.name):
            print("Deleting " + obj.name)
            bpy.data.objects.remove(obj, do_unlink=True)

    # deletes all ends with "_low"
    pattern = re.compile(r'_low')
    for obj in bpy.data.objects:
        if pattern.match(obj.name):
            print("Deleting " + obj.name)
            bpy.data.objects.remove(obj, do_unlink=True)

def merge_model_segments(mergedSegments, modelName): 
    # Ensure we're in Object mode
    bpy.ops.object.mode_set(mode='OBJECT')

    # Deselect all objects
    bpy.ops.object.select_all(action='DESELECT')

    for segment in mergedSegments:
        segment.select_set(True)
        
    modelObject = None
    
    if len(bpy.context.selected_objects) > 0:
        bpy.context.view_layer.objects.active = bpy.context.selected_objects[0]
        
        if len(bpy.context.selected_objects) > 1:
            bpy.ops.object.join()
            
        modelObject = bpy.context.object
        
        if modelObject is not None:
            modelObject.name = modelName
            modelObject.data.name = f"{modelName}_mesh"
            
            bpy.ops.object.transforms_to_deltas(mode='ALL')
            
            modelObject.select_set(False)
            bpy.context.view_layer.objects.active = None            
    mergedSegments.clear()
    
    bpy.ops.object.select_all(action='DESELECT')
    
    return modelObject

def combile_LOD():
    detailSegments = []
    colliderSegments = []

    for segment in bpy.data.objects:
        if segment.type == 'MESH' and len(segment.data.polygons) > 0:
            segmentName = segment.name
        
            if segmentName.lower().endswith(( 'lod0', 'lod1', 'grass' )):
                detailSegments.append(segment)
            elif "low_interior" in segmentName.lower():
                # delete low_interior
                bpy.data.objects.remove(segment, do_unlink=True)
            elif re.search(r'low|lod2|lod3', segmentName.lower()):
                colliderSegments.append(segment)
            else:
                # Not sure what it is. Add it to detailSegments (cc_railroad)
                detailSegments.append(segment)
    
    # merge model
    print("merging LOD segments")
    modelObject = merge_model_segments(detailSegments, "model")

    colliderObject = None
    # colliderObject = merge_model_segments(colliderSegments, "collider")

    return [modelObject, colliderObject]

def split_into_nanite_sections(modelObject):
    sectionObjects = []
     
    if modelObject is None:
        print("Warning: cannot separate mesh into nanite section. An invalid mesh object was provided.")
        return sectionObjects
    
    #select the model object and set as active
    modelObject.select_set(True)
    bpy.context.view_layer.objects.active = modelObject
    
    if bpy.context.mode != "EDIT_MESH":
        bpy.ops.object.editmode_toggle()
        
    # nanite supports up to 64 unique materials per mesh
    # we will subdivide the mesh into sections with equal number of materials
    modelObject.select_set(True)
    
    bpy.ops.mesh.select_all(action='DESELECT')
    
    numMaterials = len(modelObject.data.materials)
    if numMaterials > 64:
        numMaterialSections = math.ceil(numMaterials / 64)
        maxNumSectionMaterials = math.ceil(numMaterials / numMaterialSections)
        
        for i in range(maxNumSectionMaterials, maxNumSectionMaterials * numMaterialSections, maxNumSectionMaterials):
            hasSelectedPolygons = False
            for k in range(i - maxNumSectionMaterials, i):
                bpy.context.object.active_material_index = k
                if 'FINISHED' in bpy.ops.object.material_slot_select():
                    hasSelectedPolygons = True
            
            if hasSelectedPolygons:    
                bpy.ops.mesh.separate(type='SELECTED')
           
                bpy.ops.mesh.select_all(action='DESELECT')
            
        sectionObjects = bpy.context.selected_objects
    else:
        sectionObjects.append(modelObject)
    
    bpy.ops.object.editmode_toggle()
    
    bpy.ops.object.select_all(action='DESELECT')
    
    return sectionObjects

def generate_report(model_file_path, model_objects, collider_object, root_object):
    report = {}

    # Load Material Info
    if(model_file_path != None):
        # Get path of the material file
        source_dir, filename = os.path.split(model_file_path)
        name, _ = os.path.splitext(filename)
        material_info = os.path.join(source_dir, "MaterialInfos", f"{name}.json")
        if not os.path.exists(material_info):
            print("Material info file not found!")
        else:
            with open(material_info, 'r') as f:
                report["Materials"] = json.load(f)
    else:
        print("No model file path provided. Skipping material info.")

    # Get Mesh Info
    report["ModelNames"] = []
    for obj in model_objects:
        report["ModelNames"].append(obj.name)

    # Get Collider Info
    if collider_object:
        report["ColliderName"] = collider_object.name
    else:
        report["ColliderName"] = "None"

    # Get Dependency Info
    report["ChildModels"] = []
    for obj in bpy.data.objects:
        if obj.type == 'EMPTY' and obj.name.startswith('SOCKET_EXT_'):
            # drop last _### from name and SOCKET_EXT_
            child = obj.name.rsplit('_', 1)[0][11:]
            # check if exist already
            if child not in report["ChildModels"]:
                report["ChildModels"].append(child)

    # debug print
    # print(json.dumps(report, indent=4))

    return report


def process(model_file_path = None):
    # # Task 0: Delete Dummy
    # delete_dummy()

    # Task: Convert Dummy to Plane
    convert_dummy_to_plane()

    # Task: Combile LOD meshes
    model_object, collider_object = combile_LOD()

    # Task: split model into nanite sections
    model_objects = split_into_nanite_sections(model_object)
    
    # Task: Automatically select an armature and convert its skeleton to empties.
    root_object = next((obj for obj in bpy.context.scene.objects if obj.type == 'ARMATURE'), None)
    if root_object:
        root_object = skeleton_to_empties(root_object)
    else:
        raise ValueError("No armature found in the scene!")
    
    # Task: Make the model_object[0] the parent of root_object. Preserve root_object's world space coordinates.
    if model_objects:
        make_empty_as_child(model_objects[0], root_object)
        
    # Task: Move PL# to root/pointlight
    move_PL_to_pointlight(root_object)

    # Task: move decal and create socket
    move_decallocator_to_decal(root_object)

    # Task: move grass and create socket
    move_grasslocator_to_grass(root_object)
             
    # Task: rename ext_ & decal_ to SOCKET_
    convert_ext_to_socket()
    convert_decal_to_socket()

    # Task: Generate Report
    report = generate_report(model_file_path, model_objects, collider_object, root_object)

    return report


def export(output_file_path, report):
    # Export Path
    output_file_dir = os.path.dirname(output_file_path)
    model_file_name = os.path.basename(output_file_path)
    model_name = os.path.splitext(model_file_name)[0]

    os.makedirs(output_file_dir, exist_ok=True)

    # Export Model
    bpy.ops.export_scene.fbx(filepath=output_file_path, check_existing=False, use_selection=False, mesh_smooth_type='FACE', use_custom_props=True, path_mode='RELATIVE', axis_forward='-Y', axis_up='Z')

    # Export Report
    output_report_path = os.path.join(output_file_dir, model_name)  + '.json'
    with open(output_report_path, 'w') as f:
        json.dump(report, f, indent=4)

def main():
    # Print all argv
    print("argv: " + str(sys.argv))

    # Look for "-SOURCE in argv. If found, use the next argument as the source file path.
    source_file_path = None
    for i in range(len(sys.argv)):
        if sys.argv[i] == "-SOURCE":
            source_file_path = sys.argv[i + 1]
            break   \

    # Look for "-OUTPUT in argv. If found, use the next argument as the destination file path.
    output_file_path = None
    for i in range(len(sys.argv)):
        if sys.argv[i] == "-OUTPUT":
            output_file_path = sys.argv[i + 1]
            break

    # If Source file path is not found, skip import, process the current scene.
    if source_file_path:
        print("Processing source file: " + source_file_path)
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.outliner.orphans_purge(do_recursive=True)
        if not os.path.exists(source_file_path):
            raise ValueError("Source file not found!")

        # Import the source file
        bpy.ops.import_scene.fbx(filepath=source_file_path, use_custom_normals=True)

    else:
        print("Source file path not found. Processing current scene.")

    bpy.context.window.scene = bpy.data.scenes['Scene']
    report = process(source_file_path)

    # If Output file path is found, export the processed scene.
    if output_file_path:
        print("Exporting to: " + output_file_path)
        export(output_file_path, report)



    
    

# Call the main function
main()