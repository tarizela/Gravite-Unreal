#include "GravityBlueprintTools.h"

#include <AssetToolsModule.h>
#include <Modules/ModuleManager.h>

DEFINE_LOG_CATEGORY(LogGravityBlueprintTools)

void UGravityBlueprintTools::CreateBlueprintForMesh(const FString& MeshAssetPath)
{
	FAssetToolsModule& assetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");

	IAssetTools& assetTools = assetToolsModule.Get();

	UE_LOG(LogGravityBlueprintTools, Display, TEXT("%s"), *MeshAssetPath);
}