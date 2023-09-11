import unreal
import os

if __name__ == "__main__":
    selectedAssets = unreal.EditorUtilityLibrary.get_selected_assets()

    if len(selectedAssets) != 1:
        unreal.log_warning("Please select one static mesh asset")
        quit()

    modelPath = os.path.dirname(selectedAssets[0].get_path_name())

    unreal.GravityBlueprintTools.create_blueprint_for_mesh(modelPath)