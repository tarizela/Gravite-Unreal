import bpy
import re
import json
import os.path
import bmesh
import math
import mathutils
import zlib

class text_color:
    red = '\033[91m'
    green = '\033[92m'
    yellow = '\033[93m'
    blue = '\033[94m'
    magenta = '\033[95m'
    cyan = '\033[96m'
    
    esc = '\033[0m'
    
class message_type:
    info = 0
    warning = 1
    error = 2
    success = 3

class object_locator:
    def __init__(self, type):
        # the type of the locator
        self.type = type
        
        # name of the locator
        self.name = ''
        
        # transform matrix
        self.matrix_world = mathutils.Matrix()
        
        # socket object
        self.socket = None
        
    def to_dict(self):
        return {
            'Name' : self.name,
            'Type' : self.type
        }

class decal_locator(object_locator):
    def __init__(self):
        super().__init__('DECAL')
        
        # material that is used by the decal
        self.material_name = ''
        
    def to_dict(self):
        dict_object = super().to_dict()
        dict_object['MaterialName'] = self.material_name
        return dict_object
        
class model_locator(object_locator):
    def __init__(self):
        super().__init__('MODEL')
        
        # name of the model
        self.model_name = ''
    
    def to_dict(self):
        dict_object = super().to_dict()
        dict_object['ModelName'] = self.model_name
        return dict_object

class effect_locator(object_locator):
    def __init__(self):
        super().__init__('EFFECT')
        
        # name of the effect
        self.effect_name = ''
        
    def to_dict(self):
        dict_object = super().to_dict()
        dict_object['EffectName'] = self.effect_name
        return dict_object

class light_locator(object_locator):
    def __init__(self):
        super().__init__('LIGHT')

        # type of the light
        self.is_directional = False   
        
        # intensity of the light
        self.intensity = 0.0
        
        # RGB color vector (most likely sRGB)
        self.color = []
        
        # light falloff
        self.falloff = 0.0
        
        # range of the light
        self.range = 0.0
        
    def to_dict(self):
        dict_object = super().to_dict()
        dict_object['Directional'] = self.is_directional
        dict_object['Intensity'] = self.intensity
        dict_object['Color'] = self.color
        dict_object['Falloff'] = self.falloff
        dict_object['Range'] = self.range
        return dict_object

class debris_set:
    def __init__(self):
        # name of the debris set
        self.name = ''
        
        # all meshes of a debris set (geometry or locator meshes)
        self.meshes = []

    def to_dict(self):
        return { 'Name' : self.name, 'Meshes' : [ mesh.name for mesh in self.meshes ] }

class model_desc:
    def __init__(self):
        # name of the base model
        self.model_name = ''
        
        # path to the source fbx file
        self.source_filepath = ''
        
        # path to the source material json file
        self.source_material_info_filepath = ''
    
        # the broken model (this model is static in the game)
        self.broken_models = []
    
        # the dynamic, broken model (this model is a damaged version of the static model. It's used to allow grabbing and throwing the object with the stasis ability).
        self.broken_dynamic_models = []

        # models containing debris meshes.
        self.debris_models = []
    
        # material info
        self.model_material_info = {}
    
        # model sections with nanite support
        self.nanite_meshes = []
    
        # model sections that do not support nanite
        self.regular_meshes = []
        
        # object locators for lights, decals, models and effects
        self.object_locators = []

        # collection of names representing debris clusters
        self.debris_clusters = []

def material_supports_nanite(material_info):
    material_type = material_info['Type']

    unsupported_material_types = [
        '24', # water or translucent surface material
        '2D', # water surface material
        '28', # tree branch material
        '29', # grass blade material
        '2E'  # translucent material
    ]

    if material_type in unsupported_material_types:
        return False
    
    return True

def format_message(type, message):
    if type == message_type.info:
        ansi_color = text_color.blue
    elif type == message_type.warning:
        ansi_color = text_color.yellow
    elif type == message_type.error:
        ansi_color = text_color.red
    elif type == message_type.success:
        ansi_color = text_color.green
    else:
        ansi_color = ''

    return f'{ansi_color}{message}{text_color.esc}'

def log_message(type, message):
    print(format_message(type, message))

def create_exception(message):
    return Exception(f'{text_color.red}{message}{text_color.esc}')

class model_converter:
    def __init__(self, input_model_dir, input_model_material_dir, output_model_dir):
        self.version = ( 1, 1, 0 )
        self.input_model_dir = input_model_dir
        self.input_model_material_dir = input_model_material_dir
        self.output_model_dir = output_model_dir
        self.skipped_model_filepaths = []
        self.model_database = {}

        self.unsupported_model_filter = [ 
            'ef_', # meshes for effects, fbx and material does not contain any useful information at the moment.
            'sky_' # defines settings for the environment probe, renderer globals and the sky box. The fbx and material does not contain any useful information at the moment.
        ]

        self.check_app_version()

    def check_app_version(self):
        required_major_version, required_minor_version = ( 4, 4 )
        major_version, minor_version, _ = bpy.app.version

        if major_version != required_major_version or minor_version != required_minor_version:
            raise create_exception(f'Unsupported blender version. The converter requires blender {required_major_version}.{required_minor_version}. Your blender version is {major_version}.{minor_version}.')

    def add_skipped_model(self, filepath, reason):
        self.skipped_model_filepaths.append(( filepath, reason ))

    def create_model_database(self):
        database = {}
        
        # _brk (just a broken version of the model, it's not a breakable)
        # _brk_md<*> and _brk_md<*>_db (is a breakable subobject of the model and it's debris)
        # _brk_bk is the version of the model in the broken state
        # _brk_dy is the version of the model after grabbing it with stasis
        # _brk_db is the debris for the model 
        re_breakable_name_pattern = re.compile(rf'(.*)_brk_(bk|dy|db)+')
        re_breakable_submodel_name_pattern = re.compile(rf'(.*_brk_md\d+)_(db)')

        # initialize the database with all models
        broken_models = {}
        broken_dynamic_models = {}
        debris_models = {}

        for filename in os.listdir(self.input_model_dir):
            name, extension = os.path.splitext(filename)
            
            if extension.lower() != '.fbx':
                continue

            is_model_supported = True
            for filter_prefix in self.unsupported_model_filter:
                if name.startswith(filter_prefix):
                    is_model_supported = False
                    break

            model_filepath = os.path.join(self.input_model_dir, filename)

            if not is_model_supported:
                self.add_skipped_model(model_filepath, 'the model was manually excluded from import')
                continue
                    
            # the model must have a material info file
            model_material_info_filepath = os.path.join(self.input_model_material_dir, f'{name}.json')
            if not os.path.exists(model_material_info_filepath):
                raise create_exception(f'Model "{model_filepath}" must have a material info file "{model_material_info_filepath}".')
        
            new_model = model_desc()
            new_model.model_name = name
            new_model.source_filepath = model_filepath
            new_model.source_material_info_filepath = model_material_info_filepath

            re_search_result = re_breakable_name_pattern.match(name)

            if not re_search_result:
                re_search_result = re_breakable_submodel_name_pattern.match(name)
            
            if not re_search_result:            
                database[name] = new_model
            else:
                parent_name = re_search_result[1]
                breakable_type = re_search_result[2]
                 
                if breakable_type == 'dy':
                    if parent_name not in broken_dynamic_models:
                        broken_dynamic_models[parent_name] = []
                    broken_dynamic_models[parent_name].append(new_model)
                elif breakable_type == 'bk':
                    if parent_name not in broken_models:
                        broken_models[parent_name] = []
                    broken_models[parent_name].append(new_model)
                elif breakable_type == 'db':
                    if parent_name not in debris_models:
                        debris_models[parent_name] = []
                    debris_models[parent_name].append(new_model)
        
        # output name of breakable models that do not belong to any base model
        for parent_model_name, models in broken_dynamic_models.items():
            if parent_model_name not in database:
                for model in models:
                    log_message(message_type.warning, f'WARNING: broken dynamic (brk_dy) model "{model.model_name}" does not have a base model.')
            
        for parent_model_name, models in broken_models.items():
            if parent_model_name not in database:
                for model in models:
                    log_message(message_type.warning, f'WARNING: broken (brk_bk) model "{model.model_name}" does not have a base model.')
            
        for parent_model_name, models in debris_models.items():
            if parent_model_name not in database:
                for model in models:
                    log_message(message_type.warning, f'WARNING: debris (brk_db) model "{model.model_name}" does not have a base model.')
        
        # assign breakables to base models
        for model_name, model in database.items():
            model.broken_models.extend(broken_models.get(model_name, []))
            model.broken_dynamic_models.extend(broken_dynamic_models.get(model_name, []))
            model.debris_models.extend(debris_models.get(model_name, []))
        
        return database

    def get_scene_armatures(self):
        root_armatures = []
        additional_armatures = []
        
        for object in bpy.data.objects:
            if object.type == 'ARMATURE':
                if 'root' in object.name.lower():
                    root_armatures.append(object)
                else:
                    additional_armatures.append(object)

        # some scene only have one armature but do not name the armature 'root'
        if not root_armatures and additional_armatures:
            root_armatures.append(additional_armatures[0])
            additional_armatures.remove(root_armatures[0])

        return ( root_armatures, additional_armatures )

    def get_root_armature(self):
        root_armatures, _ = self.get_scene_armatures()
        
        return root_armatures[0]

    def create_root_armature(self):
        log_message(message_type.warning, 'WARNING: Creating new root armature...')

        bpy.ops.object.armature_add()

        armature = bpy.context.object
        armature.name = 'root'
        armature.data.name = 'root'

        bpy.ops.object.mode_set(mode='EDIT')

        # delete existing bones before creating new ones
        bpy.ops.armature.select_all(action='SELECT')
        bpy.ops.armature.delete()

        # create bones for locators
        object_locators = []
        re_locator_name_pattern = re.compile(rf'(ext_|bg_)(.*)_(root$|\d+$)')
        for object in bpy.data.objects:
            if object.type == 'EMPTY':
                match_result = re_locator_name_pattern.match(object.name)
                if match_result and match_result[2]:
                    locator_name = f'ext_{match_result[2]}_{len(object_locators)}'

                    bpy.ops.armature.bone_primitive_add(name=locator_name)
                    armature.data.edit_bones[locator_name].matrix = object.matrix_world

                    object_locators.append(locator_name)

        if object_locators:
            log_message(message_type.warning, 'WARNING: Created locator bones:')

            for name in object_locators:
                log_message(message_type.warning, f'WARNING:\t{name}')

        bpy.ops.object.mode_set(mode='OBJECT')

    def import_model(self, fbx_filepath):
        # remove objects of the previous import
        if bpy.context.scene.objects:
            bpy.context.view_layer.objects.active = bpy.context.scene.objects[0]
            bpy.ops.object.mode_set(mode='OBJECT')
            
            for object in bpy.context.scene.objects:
                object.hide_set(False)
                
            bpy.ops.object.select_all(action='SELECT')
            bpy.ops.object.delete()

        # purge orphaned objects to allow resources to keep their original names
        bpy.ops.outliner.orphans_purge(do_local_ids=True, do_linked_ids=True, do_recursive=True)
        
        try:
            bpy.ops.import_scene.fbx(filepath=fbx_filepath, primary_bone_axis='Z', use_image_search=False)
        except:
            log_message(message_type.error, f'\n\nERROR: import failed for "{fbx_filepath}"')
            self.add_skipped_model(fbx_filepath, 'import of the model file failed')  
            return False

        root_armatures, additional_armatures = self.get_scene_armatures()

        # there should be a single root armature after the import
        if len(root_armatures) != 1:
            log_message(message_type.warning, f'WARNING: Invalid number of root armatures after the import of "{fbx_filepath}". The script expects the model to have a single root armature that describes the structure of the model.')
            
            root_armatures.append(self.create_root_armature())
        
        if additional_armatures:
            log_message(message_type.warning, 'WARNING: the scene contains additional armatures besides the root armature that defines the model structure. These are the additional armatures:')
            for armature in additional_armatures:
                log_message(message_type.info, f'\t{armature.name}')
                
        return True
        
    def export_model(self, model, export_dir):
        model_export_dir = os.path.join(export_dir, model.model_name)

        log_message(message_type.info, f'Exporting model "{model.model_name}" to "{model_export_dir}".')
        
        os.makedirs(model_export_dir, exist_ok=True)
        
        exported_meshes = []
        exported_meshes.extend(model.nanite_meshes)
        exported_meshes.extend(model.regular_meshes)
        
        bpy.ops.object.select_all(action='DESELECT')
        for mesh in exported_meshes:
            mesh.select_set(True)
            
        for locator in model.object_locators:
            locator.socket.select_set(True)
        
        model_file_path = os.path.join(model_export_dir, model.model_name) + '.fbx'
        
        bpy.ops.export_scene.fbx(filepath=model_file_path, check_existing=False, use_selection=True, mesh_smooth_type='FACE', use_custom_props=True, path_mode='RELATIVE', axis_forward='-Y', axis_up='Z')
        
        model_info_file_path = os.path.join(model_export_dir, model.model_name)  + '.json'
        
        major_version, minor_version, patch_version = self.version

        model_info = {
            'Version': f'{major_version}.{minor_version}.{patch_version}',
            'Meshes': [ mesh.name for mesh in exported_meshes ],
            'BrokenModels': [ model.model_name for model in model.broken_models ],
            'BrokenDynamicModels': [ model.model_name for model in model.broken_dynamic_models ],
            'DebrisModels': [ model.model_name for model in model.debris_models ],
            'DebrisClusters': { f'Debris{idx + 1}' : model.debris_clusters[idx].to_dict() for idx in range(len(model.debris_clusters)) },
            'Locators' : { f'Locator{idx + 1}' : model.object_locators[idx].to_dict() for idx in range(len(model.object_locators)) }
        }
        
        if model.model_material_info:
            model_info['Materials'] = model.model_material_info
        
        with open(model_info_file_path, 'w') as file:
            json.dump(model_info, file, indent=4)
        
    def collect_meshes(self):
        allowed_mesh_name_postfixes = ('_lod0', '_lod1', '_grass')
        disallowed_mesh_name_postfixed = ('_low', '_lod3')
        
        # find mesh with matching name
        high_poly_meshes = []
        low_poly_meshes = []
        for mesh in bpy.data.objects:
            if mesh.type == 'MESH':
                mesh_name_lower = mesh.name.lower()
                if mesh_name_lower.endswith(allowed_mesh_name_postfixes):
                    high_poly_meshes.append(mesh)
                elif mesh_name_lower == 'dummy':
                    bpy.ops.mesh.primitive_uv_sphere_add()
                    bpy.context.object.matrix_world = mesh.matrix_world
                    low_poly_meshes.append(bpy.context.object)
                elif mesh_name_lower.startswith('low_') or mesh_name_lower.endswith(disallowed_mesh_name_postfixed):
                    # use all meshes without a filtered prefix and postfix as LOD0 meshes
                    low_poly_meshes.append(mesh)
                elif mesh.data.polygons:
                    high_poly_meshes.append(mesh)
        
        # the model does not have any valid meshes, try salvaging the conversion attempt by creating a dummy mesh 
        if not high_poly_meshes and not low_poly_meshes:
            bpy.ops.mesh.primitive_uv_sphere_add()
            low_poly_meshes.append(bpy.context.object)
        
        if high_poly_meshes:
            return high_poly_meshes
        else:
            return low_poly_meshes

    def get_sorted_scene_meshes(self):
        scene_meshes = []
        
        # our debris collection algorithm depends on the sorting behavior of blender, this is very hacky.
        for object in bpy.context.view_layer.objects:
            if object.type == 'MESH':
                scene_meshes.append(object)
            
        return scene_meshes

    def collect_debris_meshes(self):
        debris_sets = []
        
        # first collect information about the debris sets from the armature
        scene_armature = self.get_root_armature()
        re_debris_set_name_pattern = re.compile(rf'(bone\d+_)*(.*(_db)*)_\d+$')
        for bone in scene_armature.pose.bones:
            if not bone.parent:
                # debris of trees can have branches locators
                for child_bone in bone.children:
                    if 'branchlocator' not in child_bone.name.lower():
                        raise create_exception(f'Failed to collect debris meshes. Encountered an unknown locator "{child_bone.name}" in a debris set.')

                match_result = re_debris_set_name_pattern.match(bone.name)
                if match_result and match_result[2]:
                    new_set = debris_set()
                    # ensure that the debris sets have unique names
                    new_set.name = f'{match_result[2]}_{len(debris_sets)}'
                    debris_sets.append(new_set)
                else:
                    raise create_exception(f'Failed to collect debris meshes. The bone "{bone.name}" uses an unknown name structure.')

        # now assign meshes to the debris sets
        sorted_meshes = self.get_sorted_scene_meshes()
        mesh_sets = []
        re_mesh_name_pattern = re.compile(rf'(\d+_)*(.*(_db)*)_(\d*)_(\d*)$|(\d+_)*(.*(_db)*)_(\d*)$')
        
        current_mesh_set = []
        next_submesh_id = 0
        for mesh in sorted_meshes:
            # can be a graphics mesh or a branch locator
            if mesh.type == 'MESH':
                if 'branchlocator' in mesh.name.lower():
                    # make the branch locator a part of the current mesh set
                    submesh_id = next_submesh_id
                else:
                    # start a new set if the ID of the submesh cannot be determined
                    submesh_id = 0

                    match_result = re_mesh_name_pattern.match(mesh.name)
                    if match_result and match_result[5]:
                        submesh_id = int(match_result[5])

                if next_submesh_id != submesh_id:
                    # the ID sequence was interrupted, start a new set
                    mesh_sets.append(current_mesh_set)
                    current_mesh_set = []

                current_mesh_set.append(mesh)

                if sorted_meshes[-1] == mesh:
                    mesh_sets.append(current_mesh_set)

                next_submesh_id = submesh_id + 1

        # assign mesh sets to debris sets
        if len(mesh_sets) != len(debris_sets):
            raise create_exception(f'Failed to create debris mesh sets. Found "{len(debris_sets)}" debris sets, but created "{len(mesh_sets)}".')

        for idx in range(len(debris_sets)):
            debris_sets[idx].meshes = mesh_sets[idx]

        return debris_sets

    def build_branch_locator_mesh(self, mesh):
        if 'branchlocator' not in mesh.name.lower() or mesh.data.polygons:
            raise create_exception(f'The mesh "{mesh.name}" is not a valid branch locator.')
        
        if len(mesh.data.vertices) % 3:
            raise create_exception(f'Branch locator "{mesh.name}" has an invalid number of vertices. Each branch locator must have three vertices.')
        
        num_polygons = int(len(mesh.data.vertices) / 3)
    
        mesh_builder = bmesh.new()
        mesh_builder.from_mesh(mesh.data)
        mesh_builder.verts.ensure_lookup_table()
    
        for i in range(num_polygons):
            mesh_builder.faces.new([
                mesh_builder.verts[i * 3 + 0],
                mesh_builder.verts[i * 3 + 1],
                mesh_builder.verts[i * 3 + 2]
            ])
        
        mesh_builder.to_mesh(mesh.data)
        mesh_builder.free()

    def build_grass_locator_mesh(self, mesh):
        log_message(message_type.info, 'Creating polygons for grass locators...')
        
        if 'grasslocator' not in mesh.name.lower() or mesh.data.polygons:
            raise create_exception(f'The mesh "{mesh.name}" is not a valid grass locator.')
            
        if len(mesh.data.vertices) % 4:
            raise create_exception(f'Grass locator "{mesh.name}" has an invalid number of vertices. Each locator must have four vertices.')

        num_polygons = int(len(mesh.data.vertices) / 4)
        
        mesh_builder = bmesh.new()
        mesh_builder.from_mesh(mesh.data)
        mesh_builder.verts.ensure_lookup_table()
        
        for i in range(num_polygons):
            p_vertex_0 = mesh_builder.verts[i * 4 + 0] # position vertex
            l_vertex_0 = mesh_builder.verts[i * 4 + 1] # light vertex

            p_vertex_1 = mesh_builder.verts[i * 4 + 2] # position vertex
            l_vertex_1 = mesh_builder.verts[i * 4 + 3] # light vertex
    
            l_vertex_0.co = p_vertex_0.co + l_vertex_0.co
            l_vertex_1.co = p_vertex_1.co + l_vertex_1.co
    
            mesh_builder.faces.new([p_vertex_0, p_vertex_1, l_vertex_1, l_vertex_0])
            
        mesh_builder.to_mesh(mesh.data)
        mesh_builder.free()

    def build_grass_locator_meshes(self, grass_locator_meshes):
        log_message(message_type.info, 'Creating polygons for grass and branch locators...')

        for mesh in grass_locator_meshes:
            mesh_name_lower = mesh.name.lower()
            if 'grasslocator' in mesh_name_lower:
                self.build_grass_locator_mesh(mesh)
            elif 'branchlocator' in mesh_name_lower:
                self.build_branch_locator_mesh(mesh)
            else:
                create_exception(f'Cannot build locator mesh for "{mesh.name}".')

    def reduce_mesh_materials(self, model, meshes):
        log_message(message_type.info, 'Reducing number of mesh materials...')
        
        with open(model.source_material_info_filepath, 'r') as file:
            material_infos = json.load(file)

        used_materials = []
        
        # remove unused material infos
        mesh_material_slot_mapping = {}
        for mesh in meshes:
            if len(mesh.material_slots) > 1:
                raise create_exception(f'Mesh "{mesh.name}" has multiple material slots.')
            
            # we will remove slot 0 if it has a duplicate material assigned
            if mesh.material_slots:
                material_slot = mesh.material_slots[0]
                used_materials.append(material_slot.name.lower())
                
                if material_slot.name not in mesh_material_slot_mapping:
                    mesh_material_slot_mapping[material_slot.name] = [ material_slot ]
                else:
                    mesh_material_slot_mapping[material_slot.name].append(material_slot)
        
        used_material_infos = {}
        
        for material_name in material_infos.keys():
            if material_name.lower() in used_materials:
                used_material_infos[material_name] = material_infos[material_name]

        # remove duplicate materials
        duplicate_materials = []
        
        used_material_infos_list = list(used_material_infos.items())
        for i in range(len(used_material_infos_list) - 1):
            used_material_name_rhs, used_material_info_rhs = used_material_infos_list[i]
            
            # The last material in the list will be used as a replacement for the duplicate materials
            for k in range(len(used_material_infos_list) - 1, i, -1):
                used_material_name_lhs, used_material_info_lhs = used_material_infos_list[k]
                
                if used_material_info_rhs == used_material_info_lhs:
                    duplicate_materials.append((used_material_name_rhs, used_material_name_lhs))
                    break
        
        # remove material infos and material slots with duplicate materials
        for material_alias, material_name in duplicate_materials:
            log_message(message_type.info, f'Removing duplicate material: "{material_alias}" will be replaced with "{material_name}".')
            
            used_material_infos.pop(material_alias)
            
            unique_material_slot = mesh_material_slot_mapping[material_name][0]
            
            for duplicate_material_slot in mesh_material_slot_mapping[material_alias]:
                duplicate_material_slot.material = unique_material_slot.material
                    
        # remove all image nodes, we do not need them. We will use image paths from the material description during the import in UE
        for _, mesh_material_slots in mesh_material_slot_mapping.items():
            material = mesh_material_slots[0].material
            
            if material.node_tree.type == 'SHADER':
                removed_image_nodes = []
                
                for node in material.node_tree.nodes:
                    if node.type == 'TEX_IMAGE':
                        removed_image_nodes.append(node)
                        
                for node in removed_image_nodes:
                    material.node_tree.nodes.remove(node)

        return used_material_infos        

    def adjust_material_parameters(self, mesh, material_infos):
        for material_slot_index in range(len(mesh.material_slots)):
            material_slot = mesh.material_slots[material_slot_index]

            material_type = ''
            if material_slot.name in material_infos:
                material_info = material_infos[material_slot.name]
                material_type = material_info['Type']

            if material_type == '30':
                if bpy.context.mode != 'EDIT_MESH':
                    bpy.ops.object.editmode_toggle()

                bpy.context.object.active_material_index = material_slot_index

                # Select faces with the assigned material.
                bpy.ops.mesh.select_all(action='DESELECT')
                bpy.ops.object.material_slot_select()
                bpy.ops.object.editmode_toggle()

                vertices = []
                for vertex in mesh.data.vertices:
                    if vertex.select:
                        # Remapped to UE coord system
                        vertices.append(mathutils.Vector((vertex.co.x, vertex.co.z, vertex.co.y)))

                # Compute the center of the bounding sphere.
                max_bound = mathutils.Vector(vertices[0])
                min_bound = mathutils.Vector(vertices[0])

                for vertex in vertices:
                    max_bound.x = max(max_bound.x, vertex.x)
                    max_bound.y = max(max_bound.y, vertex.y)
                    max_bound.z = max(max_bound.z, vertex.z)

                    min_bound.x = min(min_bound.x, vertex.x)
                    min_bound.y = min(min_bound.y, vertex.y)
                    min_bound.z = min(min_bound.z, vertex.z)

                sphere_center = (min_bound + max_bound) * 0.5

                sphere_radius = 0.0
                for vertex in vertices:
                    sphere_radius = max(sphere_radius, (vertex - sphere_center).length)

                if vertices:
                    if 'Parameters' not in material_info:
                        material_info['Parameters'] = []

                    material_info['Parameters'].extend([ round(sphere_center.x, 3), round(sphere_center.y, 3), round(sphere_center.z, 3), round(sphere_radius, 3) ])

    def merge_meshes(self, material_infos, meshes, merged_mesh_name):
        bpy.ops.object.select_all(action='DESELECT')
        
        for mesh in meshes:
            mesh.select_set(True)
            
        merged_mesh = None
        
        if bpy.context.selected_objects:
            bpy.context.view_layer.objects.active = bpy.context.selected_objects[0]
            
            if len(bpy.context.selected_objects) > 1:
                bpy.ops.object.join()
                
            merged_mesh = bpy.context.object
            
            if merged_mesh is not None:
                merged_mesh.name = merged_mesh_name
                merged_mesh.data.name = f"{merged_mesh_name}_mesh"
                
                bpy.ops.object.transforms_to_deltas(mode='ALL')
                
                merged_mesh.select_set(False)
                bpy.context.view_layer.objects.active = None
                
            bpy.context.view_layer.objects.active = merged_mesh         
        
        bpy.ops.object.select_all(action='DESELECT')

        # update mesh dependent materials parameters
        if merged_mesh is not None:
            self.adjust_material_parameters(merged_mesh, material_infos)

        return merged_mesh

    def split_nanite_mesh_set(self, meshes, material_infos):
        opaque_meshes = []
        translucent_meshes = []
        grass_locator_meshes = []
        
        log_message(message_type.info, 'Splitting mesh set...')
        
        for mesh in meshes:
            if len(mesh.material_slots) > 1:
                raise create_exception(f'Expected the mesh "{mesh.name}" to have only a single material slot.')
            
            if mesh.material_slots:
                material_name = mesh.material_slots[0].name
                material_info = material_infos[material_name]
                material_type = material_info['Type']
            else:
                material_type = '23'
            
            if material_type == '2E':
                # translucent materials are not supported by nanite in unreal 5.5
                translucent_meshes.append(mesh)
            elif material_type == '28' or material_type == '29':
                grass_locator_meshes.append(mesh)
            elif material_type == '24' or material_type == '2D':
                # water meshes
                translucent_meshes.append(mesh)
            else:  
                opaque_meshes.append(mesh)
            
        log_message(message_type.info, f'\tNumber of opaque meshes: {len(opaque_meshes)}')
        log_message(message_type.info, f'\tNumber of translucent meshes: {len(translucent_meshes)}')
        log_message(message_type.info, f'\tNumber of grass meshes: {len(grass_locator_meshes)}')    
        
        return ( opaque_meshes, translucent_meshes, grass_locator_meshes )

    def separate_nanite_mesh(self, nanite_mesh):
        mesh_sections = [ nanite_mesh ]
        
        # nanite supports up to 64 unique materials per mesh
        # we will subdivide the mesh into sections with equal number of materials
        max_num_nanite_sections = 64
        
        num_materials = len(nanite_mesh.data.materials)
        if num_materials > max_num_nanite_sections:
            if bpy.context.mode != "EDIT_MESH":
                bpy.ops.object.editmode_toggle()
            
            bpy.ops.mesh.select_all(action='DESELECT')
            
            num_material_sets = math.ceil(num_materials / max_num_nanite_sections)
            max_num_materials_per_set = math.ceil(num_materials / num_material_sets)
            
            for i in range(max_num_materials_per_set, max_num_materials_per_set * num_material_sets, max_num_materials_per_set):
                has_selected_polygons = False
                for k in range(i - max_num_materials_per_set, i):
                    bpy.context.object.active_material_index = k
                    if 'FINISHED' in bpy.ops.object.material_slot_select():
                        has_selected_polygons = True
                
                if has_selected_polygons:    
                    bpy.ops.mesh.separate(type='SELECTED')          
                    bpy.ops.mesh.select_all(action='DESELECT')
            
            mesh_sections.extend(bpy.context.selected_objects)
            
            bpy.ops.object.editmode_toggle()

        return mesh_sections

    def create_nanite_meshes(self, model):
        log_message(message_type.info, 'Creating nanite meshes...')

        # collects meshes for lod0, lod1 and grass
        model_meshes = self.collect_meshes()
        
        # adjust materials
        material_infos = self.reduce_mesh_materials(model, model_meshes)
        
        model.model_material_info.update(material_infos)
        
        # extranct meshes with unsupported nanite materials
        opaque_meshes, translucent_meshes, grass_locator_meshes = self.split_nanite_mesh_set(model_meshes, material_infos)
        
        # merge all meshes into a single mesh
        if opaque_meshes:
            merged_opaque_mesh = self.merge_meshes(material_infos, opaque_meshes, f'{model.model_name}_opaque')
            
            # split nanite meshes into submeshes if number of materials exceeds the max allowed number of nanite sections
            nanite_mesh_sections = self.separate_nanite_mesh(merged_opaque_mesh)
            
            model.nanite_meshes.extend(nanite_mesh_sections)
            
        if translucent_meshes:
            merged_translucent_mesh = self.merge_meshes(material_infos, translucent_meshes, f'{model.model_name}_translucent')
            model.regular_meshes.append(merged_translucent_mesh)
            
        if grass_locator_meshes:
            # add polygons to the grass locator meshes so that unreal is able to import them
            self.build_grass_locator_meshes(grass_locator_meshes)
            
            merged_grass_locator_mesh = self.merge_meshes(material_infos, grass_locator_meshes, f'{model.model_name}_grass_locator')
            model.regular_meshes.append(merged_grass_locator_mesh)
            
    def create_debris_nanite_meshes(self, model):
        log_message(message_type.info, 'Creating nanite meshes for debris...')
        
        debris_sets = self.collect_debris_meshes()
        
        # adjust materials
        material_infos = self.reduce_mesh_materials(model, [ mesh for debris in debris_sets for mesh in debris.meshes ])
    
        model.model_material_info.update(material_infos)
    
        # merge meshes in every mesh set
        for debris in debris_sets:
            opaque_meshes, translucent_meshes, grass_locator_meshes = self.split_nanite_mesh_set(debris.meshes, material_infos)
            
            # create a new debris cluster
            debris_cluster = debris_set()
            debris_cluster.name = debris.name
            
            if opaque_meshes:
                merged_opaque_mesh = self.merge_meshes(material_infos, opaque_meshes, f'{model.model_name}_opaque')
                nanite_mesh_sections = self.separate_nanite_mesh(merged_opaque_mesh)
                model.nanite_meshes.extend(nanite_mesh_sections)
                
                debris_cluster.meshes.extend([mesh for mesh in nanite_mesh_sections])
            
            if translucent_meshes:
                merged_translucent_mesh = self.merge_meshes(material_infos, translucent_meshes, f'{model.model_name}_translucent')
                model.regular_meshes.append(merged_translucent_mesh)
                
                debris_cluster.meshes.append(merged_translucent_mesh)

            if grass_locator_meshes:
                # add polygons to the branch and grass locator meshes so that unreal is able to import them
                self.build_grass_locator_meshes(grass_locator_meshes)
            
                merged_grass_locator_mesh = self.merge_meshes(material_infos, grass_locator_meshes, f'{model.model_name}_grass_locator')
                model.regular_meshes.append(merged_grass_locator_mesh)

                debris_cluster.meshes.append(merged_grass_locator_mesh)

            model.debris_clusters.append(debris_cluster)
    
    def collect_locator_bones(self, type, armature):
        locator_bones = []
        
        re_model_locator_name_pattern = re.compile('.*ext_')
        re_effect_locator_name_pattern = re.compile('.*ext_ef_')
        re_name_pattern = re.compile(rf'(.*)_\d+$')
        
        for bone in armature.pose.bones:
            bone_name_lower = bone.name.lower()
            if type == 'EFFECT':
                if re_effect_locator_name_pattern.match(bone_name_lower):
                    locator_bones.append(bone)
            elif type == 'MODEL':
                if not re_effect_locator_name_pattern.match(bone_name_lower) and re_model_locator_name_pattern.match(bone_name_lower):
                    locator_bones.append(bone)
            
        for bone in armature.pose.bones:
            if not bone.parent and 'locator' in bone.name.lower():
                for child_bone in bone.children:
                    match_result = re_name_pattern.match(child_bone.name)
                    if match_result and match_result[1]:
                        locator_name_lower = match_result[1].lower()
                        # grass and decal locators are collected from objects
                        if 'grass' not in locator_name_lower and 'decal' not in locator_name_lower:
                            if type == 'EFFECT' and 'ef_' in locator_name_lower:
                                locator_bones.append(child_bone)
                            elif type == 'MODEL' and 'ef_' not in locator_name_lower:
                                locator_bones.append(child_bone)
        
        return locator_bones
        
    def create_object_locator_from_bone(self, type, bone):
        if type == 'EFFECT':
            re_effect_name_pattern = re.compile(rf'.*(ext_(ef_.*)(_\d+$))')
            name_match = re_effect_name_pattern.match(bone.name)
            if name_match and name_match[1] and name_match[2]:
                locator_name = name_match[1]
                effect_name = name_match[2]
            else:
                raise create_exception(f'Locator bone "{bone.name}" is not an effect locator.')
                
            locator = effect_locator()
            locator.effect_name = effect_name
        elif type == 'MODEL':
            re_ext_model_name_pattern = re.compile(rf'.*(ext_(.*)(_\d+$))')
            re_model_name_pattern = re.compile(rf'((.*)(_\d+$))')
            name_ext_match = re_ext_model_name_pattern.match(bone.name)
            name_match = re_model_name_pattern.match(bone.name)
            if name_ext_match and name_ext_match[1] and name_ext_match[2]:
                locator_name = name_ext_match[1]
                model_name = name_ext_match[2]
            elif name_match and name_match[1] and name_match[2]:
                locator_name = name_match[1]
                model_name = name_match[2]
            else:
                raise create_exception(f'Locator bone "{bone.name}" is not a model locator.')
            
            locator = model_locator()
            locator.model_name = model_name
  
        locator.name = locator_name
        locator.matrix_world = bone.id_data.matrix_world @ bone.matrix
        
        return locator
        
    def collect_object_locators(self):
        object_locators = []
        
        re_light_parameters_pattern = re.compile(rf'.*(PL\d*)-([\d|\.]+)-([\d|\.]+)-([\d|\.]+)-([\d|\.]+)-([\d|\.]+)-([\d|\.]+)-([\d|\.]+)-([\d|\.]+)')
        
        for object in bpy.data.objects:
            if object.type != 'EMPTY':
                continue

            object_name_lower = object.name.lower()

            if 'decallocator' in object_name_lower:
                decal = decal_locator()
                decal.name = object.name
                decal.material_name = object.name
                decal.matrix_world = object.matrix_world
                object_locators.append(decal)
            elif 'directionallight' in object_name_lower:
                light = light_locator()
                light.name = object.name
                light.is_directional = True
                light.matrix_world = object.matrix_world
                object_locators.append(light)
            elif 'pl' in object_name_lower:
                light_parameters = re_light_parameters_pattern.match(object.name)
                
                if light_parameters:
                    if light_parameters[7] != light_parameters[8] or light_parameters[7] != light_parameters[9]:
                        raise create_exception(f'Light "{object.name}" has a non uniform scale.')

                    light = light_locator()
                    light.name = light_parameters[1]
                    light.matrix_world = object.matrix_world
                    light.intensity = light_parameters[2]
                    light.color = [ light_parameters[3], light_parameters[4], light_parameters[5] ]
                    light.falloff = light_parameters[6]
                    light.range = light_parameters[7]
                    object_locators.append(light)
                    
        # collect model and effect locators from the armature
        scene_armature = self.get_root_armature()
            
        model_locator_bones = self.collect_locator_bones('MODEL', scene_armature)    
        for bone in model_locator_bones:
            object_locators.append(self.create_object_locator_from_bone('MODEL', bone))
            
        effect_locator_bones = self.collect_locator_bones('EFFECT', scene_armature)
        for bone in effect_locator_bones:
            object_locators.append(self.create_object_locator_from_bone('EFFECT', bone))
        
        return object_locators

    def collect_decal_materials(self, model, object_locators):
        decal_locators = {}

        for locator in object_locators:
            if locator.type == 'DECAL':
                decal_locators[locator.name.lower()] = locator

        decal_material_infos = {}

        if decal_locators:
            with open(model.source_material_info_filepath, 'r') as file:
                material_infos = json.load(file)

            for material_name, material_info in material_infos.items():
                for decal_locator_name in decal_locators.keys():
                    if material_name.lower() in decal_locator_name:
                        # The locator names have model locality. We need to create a hash value as a prefix for the material
                        # and decal names to be able to differentiate them during import.
                        crc32 = hex(zlib.crc32(json.dumps(material_info).encode()))[2:]

                        decal_locator = decal_locators[decal_locator_name]
                        decal_locator.material_name = f'{decal_locator.name} {crc32}'

                        decal_material_infos[decal_locator.material_name] = material_info

                        # Continue with the next material
                        break

        return decal_material_infos

    def create_sockets(self, model):
        log_message(message_type.info, 'Creating sockets...')
        
        object_locators = self.collect_object_locators()
        
        # collect decal materials
        if object_locators:
            decal_material_infos = self.collect_decal_materials(model, object_locators)
            model.model_material_info.update(decal_material_infos)

        for locator in object_locators:
            scale_x = 1.0
            scale_y = 1.0
            scale_z = 1.0

            scale_matrix = mathutils.Matrix.Diagonal((0, 0, 0, 1))

            if locator.type == 'DECAL':
                # Extract the decal scale from material
                decal_material = decal_material_infos[locator.material_name]
                scale_z *= decal_material['Parameters'][0]
                scale_y *= decal_material['Parameters'][1]
                scale_x *= decal_material['Parameters'][2]

                # Alight the locator with the projection direction (positive X) of the decal
                scale_matrix[2][0] = -scale_x # GR X -> UE -Z
                scale_matrix[1][1] =  scale_y # GR Y -> UE  Y
                scale_matrix[0][2] =  scale_z # GR Z -> UE  X
            else:
                # Align the locator with the local coord system of the submodel
                scale_matrix[0][0] = -scale_x # GR X -> UE -X
                scale_matrix[1][1] = -scale_y # GR Y -> UE -Y
                scale_matrix[2][2] =  scale_z # GR Z -> UE  Z

            locator.matrix_world = locator.matrix_world @ scale_matrix

            bpy.ops.object.empty_add(type='PLAIN_AXES', align='WORLD')
            socket = bpy.context.object
            socket.name = f'SOCKET_{locator.name}'
            socket.matrix_world = locator.matrix_world
            locator.socket = socket

        if object_locators:    
            # attach the sockets to one of the meshes
            if model.nanite_meshes:
                parent_mesh = model.nanite_meshes[0]
            elif model.regular_meshes:
                parent_mesh = model.regular_meshes[0]
            else:
                raise create_exception(f'Cannot export sockets. Sockets must be attached to one of the exported meshes but the scene does not have any meshes.')
            
            bpy.ops.object.select_all(action='DESELECT')
            for locator in object_locators:
                locator.socket.select_set(True)
            
            bpy.context.view_layer.objects.active = parent_mesh
            bpy.ops.object.parent_set(type='OBJECT')       
            bpy.ops.object.select_all(action='DESELECT')
            
            model.object_locators = object_locators

        log_message(message_type.info, f'\tNumber of object locators: {len(object_locators)}')

    def convert_model(self, model, output_model_dir):    
        log_message(message_type.info, 'Importing...')

        if self.import_model(model.source_filepath):                    
            log_message(message_type.info, 'Converting...')

            self.create_nanite_meshes(model)
        
            self.create_sockets(model)
        
            self.export_model(model, output_model_dir)
        
    def convert_debris_model(self, model, output_model_dir):
        log_message(message_type.info, 'Importing...')

        if self.import_model(model.source_filepath):
            log_message(message_type.info, 'Converting...')
        
            self.create_debris_nanite_meshes(model)
        
            self.export_model(model, output_model_dir)
        
    def process_model(self, model, output_model_dir):
        log_message(message_type.info, f'Processing: {model.source_filepath}')
        
        log_message(message_type.info, f'Converting base model: "{model.model_name}"')
        self.convert_model(model, output_model_dir)

        # convert and export the broken models
        for broken_model in model.broken_models:
            log_message(message_type.info, f'Converting broken model: "{broken_model.model_name}"')
            self.convert_model(broken_model, output_model_dir)
        
        # convert and export the broken dynamic models
        for broken_dynamic_model in model.broken_dynamic_models:
            log_message(message_type.info, f'Converting broken dynamic model: "{broken_dynamic_model.model_name}"')
            self.convert_model(broken_dynamic_model, output_model_dir)
        
        # convert and export the debris models
        for debris_model in model.debris_models:
            log_message(message_type.info, f'Converting debris model: "{debris_model.model_name}"')
            self.convert_debris_model(debris_model, output_model_dir)
            
        log_message(message_type.info, 'Done')

    def print_version(self):
        major_version, minor_version, patch_version = self.version
        log_message(message_type.info, f'Converter version: {major_version}.{minor_version}.{patch_version}.')

    def print_skipped_models(self):
        if self.skipped_model_filepaths:
            log_message(message_type.warning, 'WARNING: some unsupported models were skipped.')

            for model_filepath, skipping_reason in self.skipped_model_filepaths:
                log_message(message_type.info, f'\t{model_filepath} {text_color.yellow}reason: {skipping_reason}.{text_color.esc}')

    def convert(self):
        self.print_version()

        self.skipped_model_filepahs = []
        self.model_database = self.create_model_database()

        progress_counter = 0
        num_models = len(self.model_database)
        for _, model in self.model_database.items():
            self.process_model(model, self.output_model_dir)
            
            progress_counter += 1
            print(f'{text_color.cyan}Progress: {progress_counter}/{num_models}{text_color.esc}')

        self.print_skipped_models()

def run_converter():
    input_model_dir = 'D:/GravityAssets/fbx'
    input_model_material_info_dir = 'D:/GravityAssets/materials'  
    output_model_dir = 'D:/GravityAssets/output'
    
    converter = model_converter(input_model_dir, input_model_material_info_dir, output_model_dir)

    converter.convert()

if __name__ == '__main__':
    run_converter()