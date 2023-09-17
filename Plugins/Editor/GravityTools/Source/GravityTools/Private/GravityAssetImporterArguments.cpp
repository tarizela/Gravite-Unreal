#include "GravityAssetImporterArguments.h"

UGravityAssetImporterArguments::UGravityAssetImporterArguments()
{
	OutputMeshDir.Path = TEXT("/Game");
	OutputTextureDir.Path = TEXT("/Game/Textures");
	BaseMaterialDir.Path = TEXT("/Game/BaseMaterials");

	ImportBatchSize = 16;
	bSavePackages = false;
}