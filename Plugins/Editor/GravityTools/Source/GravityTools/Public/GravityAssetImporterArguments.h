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

	UPROPERTY(EditAnywhere, Category = RequiredArguments, meta = (EditorConfig, ContentDir, DisplayName = "Directory containing the base materials", Tooltip = "Path to the output directory in game content."))
	FDirectoryPath BaseMaterialDir;

	UPROPERTY(EditAnywhere, Category = RequiredArguments, meta = (EditorConfig, ContentDir, DisplayName = "Output mesh directory", Tooltip = "Path to the directory for storing meshes."))
	FDirectoryPath OutputMeshDir;

	UPROPERTY(EditAnywhere, Category = RequiredArguments, meta = (EditorConfig, ContentDir, DisplayName = "Output texture directory", Tooltip = "Path to the directory for storing texture assets."))
	FDirectoryPath OutputTextureDir;

	UPROPERTY(EditAnywhere, Category = OptionalArguments, meta = (EditorConfig, DisplayName = "Size of the import batch", Tooltip = "The importer will process a batch of import tasks, then save the created assets if requested and do a cleanup."))
	uint32 ImportBatchSize;

	UPROPERTY(EditAnywhere, Category = OptionalArguments, meta = (EditorConfig, DisplayName = "Save imported packages", Tooltip = "Save packages after a number of imports."))
	bool bSavePackages;
};