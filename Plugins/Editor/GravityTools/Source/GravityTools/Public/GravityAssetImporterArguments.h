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
	UPROPERTY(EditAnywhere, Category = RequiredArguments, meta = (EditorConfig, DisplayName = "Source mesh directory", Tooltip = "Path to the directory containing the source meshes."))
	FDirectoryPath SourceMeshDir;

	UPROPERTY(EditAnywhere, Category = RequiredArguments, meta = (EditorConfig, DisplayName = "Source texture directory", Tooltip = "Path to the directory containing the source textures."))
	FDirectoryPath SourceTextureDir;

	UPROPERTY(EditAnywhere, Category = RequiredArguments, meta = (EditorConfig, ContentDir, DisplayName = "Output model directory", Tooltip = "Path to the directory for storing mesh and material assets."))
	FDirectoryPath OutputModelDir;

	UPROPERTY(EditAnywhere, Category = RequiredArguments, meta = (EditorConfig, ContentDir, DisplayName = "Output texture directory", Tooltip = "Path to the directory for storing texture assets."))
	FDirectoryPath OutputTextureDir;

	UPROPERTY(EditAnywhere, Category = RequiredArguments, meta = (EditorConfig, ContentDir, DisplayName = "Directory containing the base materials", Tooltip = "Path to the output directory in game content."))
	FDirectoryPath BaseMaterialDir;

	UPROPERTY(EditAnywhere, Category = RequiredArguments, meta = (EditorConfig, DisplayName = "Number of batch saved packages", Tooltip = "The importer will batch the save request."))
	uint32 SavedPackageBatchSize;

	UPROPERTY(EditAnywhere, Category = RequiredArguments, meta = (EditorConfig, DisplayName = "Save imported packages", Tooltip = "Save packages after a number of imports."))
	bool bSavePackages;

	UPROPERTY(EditAnywhere, Category = RequiredArguments, meta = (EditorConfig, DisplayName = "Skip imported  assets", Tooltip = "Skip assets that were already imported. The importer will skip the import if the asset directory exists and contains any assets."))
	bool bSkipImportedAssets;
};