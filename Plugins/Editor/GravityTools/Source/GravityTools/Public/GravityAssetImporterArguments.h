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
	UPROPERTY(EditAnywhere, Category = RequiredArguments, meta = (EditorConfig, DisplayName = "Asset Root Directory", Tooltip = "Path to the directory containing the exported assets."))
	FDirectoryPath AssetRootDir;

	UPROPERTY(EditAnywhere, Category = RequiredArguments, meta = (EditorConfig, ContentDir, DisplayName = "Output Content Directory", Tooltip = "Path to the output directory in game content."))
	FDirectoryPath OutputContentDir;

	UPROPERTY(EditAnywhere, Category = RequiredArguments, meta = (EditorConfig, ContentDir, DisplayName = "Directory containing the base materials", Tooltip = "Path to the output directory in game content."))
	FDirectoryPath BaseMaterialDir;
};