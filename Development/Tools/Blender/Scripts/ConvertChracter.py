import bpy
import re
import os.path
    
def remove_bone_recursive(armature, parent_bone):
    for bone in parent_bone.children:
        remove_bone_recursive(armature, bone)
        
    armature.edit_bones.remove(parent_bone)

def remove_bone(armature, bone_name, recursive):    
    if not armature.is_editmode:
        prev_context_mode=bpy.context.mode
        
        bpy.context.view_layer.objects.active=bpy.data.objects[armature.name]
        bpy.ops.object.mode_set(mode='EDIT')  
    
    if bone_name in armature.edit_bones:
        bone=armature.edit_bones[bone_name]
        
        if recursive:
            remove_bone_recursive(armature, bone)
        else:
            armature.edit_bones.remove(bone)
                    
    if prev_context_mode != None:
        bpy.ops.object.mode_set(mode=prev_context_mode)

def rename_materials(object):
    if object:
        for material_slot in object.material_slots:
            material_prefix='MI_'
            
            if not material_slot.name.startswith(material_prefix):
                material=bpy.data.materials[material_slot.name]
                
                material.name=f'{material_prefix}{material.name}'
        
def adjust_materials(objects=[]):
    for variation_id, object in objects:
        if len(object.material_slots) != 1:
            raise Exception(f'Expected the mesh "{object.name}" to have only a single material slot.')
        
        # create a new material or reuse an existing one for variations of the model
        material_slot=object.material_slots[0]
        
        material_name=material_slot.name
        
        # remove existing material slot
        bpy.context.view_layer.objects.active=object
        object.active_material_index=1
        bpy.ops.object.material_slot_remove()
        
        # create the material if it does not exist
        new_material_name=f'MI_{material_name}_{variation_id}'
        
        if new_material_name not in bpy.data.materials:
            new_material=bpy.data.materials.new(name=new_material_name)
        else:
            new_material=bpy.data.materials[new_material_name]
    
        object.data.materials.append(new_material)

def import_model(fbx_filepath):
    # remove objects of the previous import
    if bpy.context.scene.objects:
        bpy.ops.object.mode_set(mode='OBJECT')
        
        for object in bpy.context.scene.objects:
            object.hide_set(False)
            
        bpy.ops.object.select_all(action='SELECT')
        bpy.ops.object.delete()

    # purge orphaned objects to allow resources to keep their original names
    bpy.ops.outliner.orphans_purge(do_local_ids=True, do_linked_ids=True, do_recursive=True)
    
    bpy.ops.import_scene.fbx(filepath=fbx_filepath, primary_bone_axis='Z')
    
def export_model_lod(fbx_filepath, object):
    if object:
        bpy.ops.object.mode_set(mode='OBJECT')
        bpy.ops.object.select_all(action='DESELECT')
    
        object.parent.select_set(True)
        object.select_set(True)
    
        bpy.ops.export_scene.fbx(filepath=fbx_filepath, check_existing=False, path_mode='RELATIVE', use_selection=True, mesh_smooth_type='FACE', use_tspace=True, use_triangles=True, add_leaf_bones=False)  

def get_mesh_objects_for_bone(bone, mesh_objects=[]):
    re_name_pattern=re.compile(rf'{bone.name}[_0-9]*$')
    re_model_variation_id_pattern=re.compile(rf'_([0-9]*)_[m|l|h]')
    
    # this is a relatively slow operation. We traverse all objects and check if their names start with the name of the bone.
    for object in bpy.data.objects:
        if object.type == 'MESH' and re_name_pattern.search(object.name):
            # extract the ID of the model variation (different faces or body setups)
            model_variation_id=re_model_variation_id_pattern.search(bone.name)
            if model_variation_id:
                # +1 to ensure that the variations start with ID=1. ID=0 is the base model and we do not want to merge the variations into the base model. 
                variation_id=int(model_variation_id[1]) + 1
            else:
                variation_id=0
            
            mesh_objects.append([variation_id, object])

def collect_objects_recursive(parent_bone, mesh_objects=[]):
    for bone in parent_bone.children:
        if bone.children:
            collect_objects_recursive(bone, mesh_objects)
        else:
            get_mesh_objects_for_bone(bone, mesh_objects)

def collect_objects(parent_bone, mesh_objects={}):    
    mesh_objects[parent_bone.name]=[]
    
    if not parent_bone.children:
        # some models (like kit01) are attached directly to the root bone. This means that the mesh does not have any LODs.
        get_mesh_objects_for_bone(parent_bone, mesh_objects[parent_bone.name])
    else:
        for bone in parent_bone.children:
            is_face_set='face_set' in bone.name
            
            if is_face_set:
                # put all face meshes in set 0 (this is a special case, we do not want the face set to become a separate mesh collection)
                collect_objects_recursive(bone, mesh_objects[parent_bone.name])
            elif not bone.children:
                get_mesh_objects_for_bone(bone, mesh_objects[parent_bone.name])
            else:
                mesh_objects_subset=[]
                
                collect_objects_recursive(bone, mesh_objects_subset)
                
                if mesh_objects_subset:
                    mesh_objects[bone.name] = mesh_objects_subset
                
    return mesh_objects

def create_lod_object(lod_object_name, objects=[]):
    if objects:
        if len(objects) > 1: 
            bpy.ops.object.select_all(action='DESELECT')
        
            for _, object in objects:
                object.select_set(True)
                
            active_object=objects[0][1]
            
            bpy.context.view_layer.objects.active=active_object           
            bpy.ops.object.join()
            
            active_object.name=lod_object_name
            active_object.data.name=f'{lod_object_name}_mesh'
            
            return active_object
        else:
            return objects[0][1]
            
def create_lod_objects(structure_root_bone, lod_level='LOD0'):
    match lod_level:
        case 'LOD0':
            re_lod_pattern=re.compile(r'_h[_]*[^a-zA-Z]*$')
        case 'LOD1':
            re_lod_pattern=re.compile(r'_m[_]*[^a-zA-Z]*$')
        case 'LOD2':
            re_lod_pattern=re.compile(r'_l[_]*[^a-zA-Z]*$')
    
    mesh_object_sets={}
    
    for bone in structure_root_bone.children:
        if re_lod_pattern.search(bone.name):            
            mesh_object_sets.update(collect_objects(bone, mesh_object_sets))
    
    lod_objects=[]
    
    for model_name, mesh_objects in mesh_object_sets.items():
        # assign different material to variations of the model
        adjust_materials(mesh_objects)
        
        lod_object=create_lod_object(model_name, mesh_objects)
        
        rename_materials(lod_object)
        
        lod_objects.append(lod_object)
        
    return lod_objects
    
def convert_model_internal(fbx_filepath):
    # there should be only a single armature after the import
    if len(bpy.data.armatures) != 1:
        raise Exception('Invalid number of armatures after the import! The script expects the model to have a single armature.')

    model_armature=bpy.data.armatures[0]
    
    # there must exist a top level object with the same name as the armature
    if model_armature.name not in bpy.data.objects:
        raise Exception('Expected the scene to have an object with the same name as the armature.')
        
    top_level_object=bpy.data.objects[model_armature.name]
    
    if top_level_object.parent != None:
        raise Exception(f'Expected the object "{top_level_object.name}" with the armature name "{model_armature.name}" to be the top level object of the scene.')
        
    # the armature must contain a root bone with the name loc_top
    root_bone_name='loc_top'
    
    if root_bone_name not in model_armature.bones:
        raise Exception(f'Expected the armature to have a bone with the name "{root_bone_name}."')
        
    if model_armature.bones[root_bone_name].parent != None:
        raise Exception(f'Expected the bone "{root_bone_name}" to be the root bone of the armature.')
    
    # create LODs for object sets
    model_structure_root_bone_name='model_top'
    
    model_objects_lod0=create_lod_objects(model_armature.bones[model_structure_root_bone_name], 'LOD0')
    model_objects_lod1=create_lod_objects(model_armature.bones[model_structure_root_bone_name], 'LOD1')
    model_objects_lod2=create_lod_objects(model_armature.bones[model_structure_root_bone_name], 'LOD2')
    
    # remove the model structure rig
    remove_bone(model_armature, model_structure_root_bone_name, True)
    
    # remove the loc_top bone
    remove_bone(model_armature, root_bone_name, False)
    
    # rename top level object and armature
    bpy.data.objects[model_armature.name].name = root_bone_name
    model_armature.name='body'
    
    return [model_objects_lod0, model_objects_lod1, model_objects_lod2]
    
def convert_model(fbx_import_filepath, fbx_export_dir):
    fbx_filename = os.path.basename(os.path.splitext(fbx_import_filepath)[0])

    print(f'processing: {fbx_import_filepath}')
    
    print('importing...')
    
    import_model(fbx_import_filepath)
    
    print('converting...')
    
    model_lods=convert_model_internal(fbx_import_filepath)
    
    for idx in range(len(model_lods)):            
        for model in model_lods[idx]:
            fbx_export_directory=os.path.join(fbx_export_dir, f'{fbx_filename}_LOD{idx}')
            
            if not os.path.exists(fbx_export_directory):
                os.makedirs(fbx_export_directory)
            
            fbx_export_filepath=os.path.join(fbx_export_directory, f'{model.name}.fbx')
        
            print(f'exporting... {fbx_export_filepath}')
        
            export_model_lod(fbx_export_filepath, model)
    
if __name__ == '__main__':
    # collection of models to convert. The fbx files for LODs will be stored in subdirectories.
    # (<path to the imported fbx>, <path for storing the converted models>)
    conversion_tasks=[
        ('D:/GravityAssets/Characters/Uni/Source/uni02_base.fbx', 'D:/GravityAssets/Characters/Uni/Uni01')
    ]
    
    for fbx_import_filepath, fbx_export_dir in conversion_tasks:
        convert_model(fbx_import_filepath, fbx_export_dir)
        