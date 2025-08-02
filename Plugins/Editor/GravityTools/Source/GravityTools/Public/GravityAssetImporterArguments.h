#pragma once

#include <EditorConfigBase.h>

#include "GravityAssetImporterArguments.generated.h"

UCLASS(EditorConfig = "GravityAssetImporter")
class UGravityAssetImporterArguments : public UEditorConfigBase
{
	GENERATED_BODY()

public:
	UGravityAssetImporterArguments();

public:
	// GravityAssetTools
	UPROPERTY(VisibleAnywhere, Category = GravityAssetTools, meta = (EditorConfig, DisplayName = "Version", Tooltip = "This is the version of the Gravity Asset Tools."))
	FString GravityAssetToolsVersion;

	// RequiredArguments
	UPROPERTY(EditAnywhere, Category = RequiredArguments, meta = (EditorConfig, DisplayName = "Source mesh directory", Tooltip = "Path to the directory containing the source meshes."))
	FDirectoryPath SourceMeshDir;

	UPROPERTY(EditAnywhere, Category = RequiredArguments, meta = (EditorConfig, DisplayName = "Source texture directory", Tooltip = "Path to the directory containing the source textures."))
	FDirectoryPath SourceTextureDir;

	UPROPERTY(EditAnywhere, Category = RequiredArguments, meta = (EditorConfig, ContentDir, DisplayName = "Directory containing the base materials", Tooltip = "Path to the output directory in game content."))
	FDirectoryPath BaseMaterialDir;

	UPROPERTY(EditAnywhere, Category = RequiredArguments, meta = (EditorConfig, ContentDir, DisplayName = "Output mesh directory", Tooltip = "Path to the directory for storing meshes."))
	FDirectoryPath OutputMeshDir;

	UPROPERTY(EditAnywhere, Category = RequiredArguments, meta = (EditorConfig, ContentDir, DisplayName = "Output texture directory", Tooltip = "Path to the directory for storing texture assets."))
	FDirectoryPath OutputTextureDir;

	UPROPERTY(EditAnywhere, Category = RequiredArguments, meta = (EditorConfig, ContentDir, DisplayName = "Output material directory", Tooltip = "Path to the directory for storing material instance assets."))
	FDirectoryPath OutputMaterialDir;

	// OptionalArguments
	UPROPERTY(EditAnywhere, Category = OptionalArguments, meta = (EditorConfig, DisplayName = "Size of the import batch", Tooltip = "The importer will process a batch of import tasks, then save the created assets if requested and do a cleanup."))
	uint32 ImportBatchSize;

	UPROPERTY(EditAnywhere, Category = OptionalArguments, meta = (EditorConfig, DisplayName = "Save imported packages", Tooltip = "Save packages after a number of imports."))
	bool bSavePackages;

	UPROPERTY(EditAnywhere, Category = OptionalArguments, meta = (EditorConfig, DisplayName = "Save the extracted parallax maps", Tooltip = "Save parallax maps that were extracted from the alpha channel of the normal maps. The parallax map will be saved to the directory of the normal map."))
	bool bSaveExtractedParallaxMaps;

	UPROPERTY(EditAnywhere, Category = OptionalArguments, meta = (EditorConfig, DisplayName = "Patch assets", Tooltip = "Patch existing assets instead of importing them."))
	bool bPatchAssets;
};