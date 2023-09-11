import unreal
import os

blueprint_dir = "/Game/Blueprints"
#Get current selected staticMesh
selected_assets = unreal.EditorUtilityLibrary.get_selected_assets()

if len(selected_assets) != 1:
    unreal.log_warning("Please select one static mesh asset")
    quit()

modelFilePath = selected_assets[0].get_path_name()
print("Model File path: " + modelFilePath)

# model name is the folder name
modelName = os.path.basename(os.path.dirname(modelFilePath))
print("Model name: " + modelName)

# root path is the folder that contains the model folder
rootPath = os.path.dirname(os.path.dirname(modelFilePath))
print("Root path: " + rootPath)

# Get the asset tools module
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

# Create a Blueprint factory
blueprint_factory = unreal.BlueprintFactory()
blueprint_factory.set_editor_property("ParentClass", unreal.Actor)

def getObjectFromSubHandle(sub_handle: unreal.SubobjectDataHandle) -> unreal.Object:
    BFL = unreal.SubobjectDataBlueprintFunctionLibrary
    return BFL.get_object(BFL.get_data(sub_handle))

def addComponentToBlueprint(blueprint: unreal.Blueprint,  new_class, name: str ) -> ( unreal.SubobjectDataHandle):
    subsystem: unreal.SubobjectDataSubsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    root_data_handle: unreal.SubobjectDataHandle = subsystem.k2_gather_subobject_data_for_blueprint(context=blueprint)[0]

    sub_handle, fail_reason = subsystem.add_new_subobject(
        params=unreal.AddNewSubobjectParams(
            parent_handle=root_data_handle,
            new_class=new_class,
            blueprint_context=blueprint))
    
    if not fail_reason.is_empty():
        raise Exception("ERROR from sub_object_subsystem.add_new_subobject: {fail_reason}")

    subsystem.rename_subobject(handle=sub_handle, new_name=unreal.Text(name))
    subsystem.attach_subobject(owner_handle=root_data_handle, child_to_add_handle=sub_handle)

    return sub_handle
def getComponentToBlueprint(blueprint: unreal.Blueprint):
    subsystem: unreal.SubobjectDataSubsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    return subsystem.k2_gather_subobject_data_for_blueprint(context=blueprint)

def createBluepoint(modelName):
    modelPath = rootPath + "/" + modelName
    print("Model name: " + modelName)
    try:
        # check if there is a blueprint in the modelFilePath already
        
        blueprintName = "BP_" + modelName
        blueprintPath = blueprint_dir + "/" + blueprintName
        print("Blueprint path: " + blueprintPath)

        if unreal.EditorAssetLibrary.does_asset_exist(blueprintPath):
            # unreal.log_warning("Blueprint already exists")
            # return True
            unreal.log_warning("Blueprint already exists, deleting it")
            unreal.EditorAssetLibrary.delete_asset(blueprintPath)
        
        # create blueprint actor
        blueprint: unreal.Blueprint = asset_tools.create_asset(blueprintName, blueprint_dir, None, blueprint_factory)

        
        assets = unreal.EditorAssetLibrary.list_assets(modelPath, False, False)

        # add static mesh component of the model
        for idx, assetPath in enumerate(assets):
            print("Asset: " + assetPath)

            assetData = unreal.EditorAssetLibrary.find_asset_data(assetPath)
            assetClass = assetData.get_class().get_name()
            print("Class: " + assetClass)

            if assetClass != "StaticMesh":
                continue

            assetName = assetData.get_asset().get_name()
            print("Name: " + assetName)

            # if name includes low or LOD2 or LOD3, skip it
            if "low" in assetName or "LOD2" in assetName or "LOD3" in assetName:
                print("Skip: " + assetName + " because it is low LOD")
                continue

            subHandle = addComponentToBlueprint(blueprint=blueprint,  new_class=unreal.StaticMeshComponent,  name=modelName + "_" + str(idx))
            staticMeshComponent = getObjectFromSubHandle(subHandle)
            assert isinstance(staticMeshComponent, unreal.StaticMeshComponent)

            mesh = unreal.EditorAssetLibrary.load_asset(assetPath)
            staticMeshComponent.set_static_mesh(new_mesh=mesh)

            print("Added Static Mesh: " + assetPath)

        # Get Each Mesh Component in the blueprint
        componentHandles = getComponentToBlueprint(blueprint)
        for componentHandle in componentHandles:
            component = getObjectFromSubHandle(componentHandle)
            print("Component: " + component.get_name())
            print("Component Details:" + str(component))

            if not isinstance(component, unreal.SceneComponent):
                print("Not a SceneComponent")
                continue

            sockets = component.get_all_socket_names()
            for socket in sockets:
                socketName = str(socket)
                print("Socket: " + socketName)

                if socketName.startswith("EXT_"): # External reference
                    # Everything after EXT_
                    childModelName = socketName[4:]
                    # Everything before last _
                    childModelName = childModelName.rsplit('_', 1)[0]

                    childActorPath = blueprint_dir + "/BP_" + childModelName

                    if not unreal.EditorAssetLibrary.does_asset_exist(childActorPath):
                        print("Create Child Blueprint: " + childModelName)
                        createBluepoint(childModelName)

                    if not unreal.EditorAssetLibrary.does_asset_exist(childActorPath):
                        print("Failed to create Child Blueprint: " + childModelName)
                        continue
                    
                    print("Load Child Blueprint: " + childActorPath)
                    childActorBP = unreal.EditorAssetLibrary.load_asset(childActorPath)

                    subHandle = addComponentToBlueprint(blueprint=blueprint,  new_class=unreal.ChildActorComponent,  name=socketName[4:])
                    childActorComponent = getObjectFromSubHandle(subHandle)
                    print("Child Actor Component: " + str(childActorComponent))
                    assert isinstance(childActorComponent, unreal.ChildActorComponent)

                    childActorComponent.set_child_actor_class(childActorBP.generated_class())
                    childActorComponent.k2_attach_to(component, socket, unreal.AttachLocation.SNAP_TO_TARGET_INCLUDING_SCALE, False)

        # SAVE
        unreal.EditorAssetLibrary.save_asset(blueprintPath)

        return blueprintPath
        


    except: 
        unreal.log_warning("Failed to create blueprint for " + modelName)
        raise
        return False


def main():
    createBluepoint(modelName)
     

if __name__ == "__main__":
    main()
