import unreal

if __name__ == "__main__":
    importTask = unreal.GravityAssetImportTask()

    importTask.set_editor_property("source_mesh_dir", "C:/Users/Zetta/Desktop/UE5Imports/Export/airconditioner_kk_01")
    importTask.set_editor_property("source_texture_dir", "C:/Users/Zetta/Desktop/UE5Imports/Textures")
    importTask.set_editor_property("base_material_dir", "/Game/Materials/BaseMaterials")
    importTask.set_editor_property("output_mesh_dir", "/Game/Architecture")
    importTask.set_editor_property("output_texture_dir", "/Game/Textures")

    unreal.GravityAssetTools.import_asset_tasks([importTask])