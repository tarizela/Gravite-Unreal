#include "GravityAssetImporterArguments.h"

#include "GravityAssetTools.h"

UGravityAssetImporterArguments::UGravityAssetImporterArguments()
{
	GravityAssetToolsVersion = UGravityAssetTools::GetVersionString();

	OutputMeshDir.Path = TEXT("/Game/Meshes");
	OutputTextureDir.Path = TEXT("/Game/Textures");
	OutputMaterialDir.Path = TEXT("/Game/Materials");

	BaseMaterialDir.Path = TEXT("/Game/BaseMaterials");

	ImportBatchSize = 16;
	bSavePackages = false;
	bSaveExtractedParallaxMaps = false;
	bPatchAssets = false;
}