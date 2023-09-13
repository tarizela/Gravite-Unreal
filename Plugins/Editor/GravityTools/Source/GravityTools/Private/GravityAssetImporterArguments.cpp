#include "GravityAssetImporterArguments.h"

UGravityAssetImporterArguments::UGravityAssetImporterArguments()
{
	OutputModelDir.Path = TEXT("/Game");
	OutputTextureDir.Path = TEXT("/Game");
	BaseMaterialDir.Path = TEXT("/Game");

	bSkipImportedAssets = false;
	bSavePackages = false;
	SavedPackageBatchSize = 16;
}