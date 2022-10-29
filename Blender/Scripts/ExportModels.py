import os
import bpy
import json
import math

class ModelExportInfo:
    ModelObjects = []
    ColliderObject = None
    MaterialInfos = []

def get_scene_armatures():
    armatures = []
    for object in bpy.context.scene.objects:
        if object.type == 'ARMATURE':
            armatures.append(object)
            
    return armatures

def create_sockets(modelObject):
    socketObjects = []
    
    # create sockets from armature 
    armatures = get_scene_armatures()
    
    armatureBones = []
    
    if len(armatures) > 0:
        if len(armatures) == 1:
            armatureObject = armatures[0]        
            armatureObject.select_set(True)
            bpy.context.view_layer.objects.active = armatureObject
            armatureBones = armatureObject.pose.bones      
        else:
            print(f"Warning: the model has multiple armatures")
            return socketObjects
    
    for bone in armatureBones:
        lowcaseBoneName = bone.name.lower()
        if lowcaseBoneName.startswith(('ext_', 'decal_')):
            if lowcaseBoneName.startswith('ext_'):
                socketName = f'SOCKET_{bone.name[4::]}'
            else:
                socketName = f'SOCKET_{bone.name[6::]}'
            
            bpy.ops.object.empty_add(type='PLAIN_AXES', align='WORLD', location=(0, 0, 0), scale=(1, 1, 1))
            
            socketObject = bpy.context.object
            
            socketObject.name = socketName
            socketObject.matrix_world = bone.id_data.matrix_world @ bone.matrix
            
            socketObjects.append(socketObject)
    
    #create sockets from locator objects
    # @todo: create sockets for decals
    
    for socketObject in socketObjects:
        socketObject.select_set(True)
    
    if len(bpy.context.selected_objects) > 0:
        bpy.context.view_layer.objects.active = modelObject
            
        bpy.ops.object.parent_set(type='OBJECT')
        
        bpy.ops.object.select_all(action='DESELECT')
    
    return socketObjects

def merge_model_segments(mergedSegments, modelName): 
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

def load_material_infos(filePath):
    materialInfos = {}

    if not os.path.exists(filePath):
        print(f"Warning: the fbx does not have a material info file!")
        return materialInfosString
    
    with open(filePath, 'r') as fd:
        materialInfos = json.load(fd)
    
    return materialInfos

def remove_unused_materials(meshObject, materialInfos):
    usedMaterialNames = []
    
    for material in meshObject.materials:
        usedMaterialNames.append(material.name.lower())
    
    definedMaterialNames = materialInfos.keys()
    
    usedMaterialInfos = {}
    
    for materialName in definedMaterialNames:
        if materialName.lower() in usedMaterialNames:
            usedMaterialInfos[materialName] = materialInfos[materialName]
            
    materialInfos.clear()
    materialInfos.update(usedMaterialInfos)

def setup_materials(meshObject, materialInfos):
    if meshObject is None:
        print(f"Warning: failed to setup materials. An invalid mesh object was provided.")
        return
    
    remove_unused_materials(meshObject, materialInfos)
    
    # remove all image nodes, we do not need them. We will use image paths from the material description during the import in UE
    for material in meshObject.materials:
        if material.node_tree.type == 'SHADER':
            removedImageNodes = []
            
            for node in material.node_tree.nodes:
                if node.type == 'TEX_IMAGE':
                    removedImageNodes.append(node)
                    
            for node in removedImageNodes:
                material.node_tree.nodes.remove(node)

def split_into_nanite_sections(modelObject):
    sectionObjects = []
    
    if modelObject is None:
        print("Warning: cannot separate mesh into nanite section. An invalid mesh object was provided.")
        return sectionObjects
    
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

def prepare_model_for_export(modelName, materialInfoFilePath, modelExportInfo):    
    if bpy.context.mode != "OBJECT":
        bpy.ops.object.editmode_toggle()
    
    bpy.ops.object.select_all(action='DESELECT')
    
    print(f"Loading material infos: {materialInfoFilePath}")
    materialInfos = load_material_infos(materialInfoFilePath)
    
    # find model and collider segments
    detailSegments = []
    colliderSegments = []
    
    for segment in bpy.data.objects:
        if segment.type == 'MESH' and len(segment.data.polygons) > 0:
            segmentName = segment.name
        
            if segmentName.lower().endswith(( 'lod0', 'lod1', 'grass' )):
                detailSegments.append(segment)
            elif 'low_model' in segmentName.lower():
                colliderSegments.append(segment)
    
    # merge model
    print("merging LOD segments")
    modelObject = merge_model_segments(detailSegments, modelName)
    
    socketObjects = []
    
    if modelObject is None:
        print(f"Warning: the model has no render mesh!")
    else:
        # create sockets
        print("creating sockets from armature")      
        socketObjects = create_sockets(modelObject)
        
        print("setup materials")        
        setup_materials(modelObject.data, materialInfos)
        modelExportInfo.MaterialInfos = materialInfos
        
        print("split into nanite sections")
        modelExportInfo.ModelObjects = split_into_nanite_sections(modelObject)
        
    # merge colliders
    print("merging collider segments")
    colliderObject = None # merge_model_segments(colliderSegments, f"{modelName}_collider")
    
    if colliderObject is None:
        print(f"Warning: the model has no collider mesh!")
    else:
        modelExportInfo.ColliderObject = colliderObject
        
    # delete unused objects
    bpy.ops.object.select_all(action='SELECT')
    
    for model in modelExportInfo.ModelObjects:
        model.select_set(False)
        
    for socket in socketObjects:
        socket.select_set(False)            
            
    if colliderObject is not None:
        colliderObject.select_set(False)
        
    bpy.ops.object.delete(use_global=False, confirm=False)

def export_model(exportDir, modelDatabeseDir, materialDatabaseDir, modelName):
    modelFilePath = os.path.join(modelDatabeseDir, modelName) + '.fbx'
    materialInfoFilePath = os.path.join(materialDatabaseDir, modelName) + '.json'
    
    print(f"------------------------------------------------------------------")
    print(f"Processing model: {modelFilePath}")
    
    modelExportInfo = ModelExportInfo()
    
    if not os.path.exists(modelFilePath):
        print(f"Warning: invalid filepath '{modelFilePath}'")
    
    bpy.ops.import_scene.fbx(filepath=modelFilePath, use_custom_normals=True)
    
    modelName = os.path.basename(modelFilePath).split('.')[0]
    
    prepare_model_for_export(modelName, materialInfoFilePath, modelExportInfo)
    
    # export model
    modelExportDir = os.path.join(exportDir, modelName)

    print(f"Exporting model: {modelExportDir}")
    
    os.makedirs(modelExportDir, exist_ok=True)
    
    bpy.ops.object.select_all(action='SELECT')
    
    if len(modelExportInfo.ModelObjects) > 0:
        modelFilePath = os.path.join(modelExportDir, modelName) + '.fbx'
    
        bpy.ops.export_scene.fbx(filepath=modelFilePath, check_existing=False, use_selection=True, mesh_smooth_type='FACE', use_custom_props=True, path_mode='RELATIVE', axis_forward='-Y', axis_up='Z')
    
        modelInfoFilePath = os.path.join(modelExportDir, modelName)  + '.json'
    
        modelInfo = {
        'ModelNames': [ modelObject.name for modelObject in modelExportInfo.ModelObjects ],
        'ColliderName': modelExportInfo.ColliderObject.name if modelExportInfo.ColliderObject is not None else "None",
        'Materials': modelExportInfo.MaterialInfos
        }
    
        with open(modelInfoFilePath, 'w') as fd:
            json.dump(modelInfo, fd, indent=4)
    
    # cleanup object
    bpy.ops.object.delete(use_global=False, confirm=False)
    bpy.ops.outliner.orphans_purge(do_recursive=True)
    
if __name__ == '__main__':
    modelListFilePath = 'C:/Users/Zetta/Desktop/UE5Imports/LCModels.txt'
    exportDir = 'C:/Users/Zetta/Desktop/UE5Imports/Export'
    
    modelDatabaseDir = 'C:/Users/Zetta/Documents/Unity Projects/GravityRush-VR-Experimental/Assets/Models'
    materialDatabaseDir = 'C:/Users/Zetta/Documents/Unity Projects/GravityRush-VR-Experimental/Assets/Models/MaterialInfos'
    
    modelList = []
    with open(modelListFilePath, 'r') as fd:
        modelList = fd.read().splitlines()
        
    #for modelName in modelList:
    modelName = 'po_a_01a'
    #modelName = 'building_sk_03'
    
    bpy.ops.outliner.orphans_purge(do_recursive=True)
    
    export_model(exportDir, modelDatabaseDir, materialDatabaseDir, modelName)