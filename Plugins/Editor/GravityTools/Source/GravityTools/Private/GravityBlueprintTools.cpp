#include "GravityBlueprintTools.h"

#include <AssetToolsModule.h>
#include <Modules/ModuleManager.h>
#include "Blueprint/BlueprintSupport.h"
#include "Factories/BlueprintFactory.h"
#include "Engine/SimpleConstructionScript.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY(LogGravityBlueprintTools)

void UGravityBlueprintTools::CreateBlueprintForMesh(const FString &modelPath)
{
	FAssetToolsModule &assetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");

	IAssetTools &assetTools = assetToolsModule.Get();

	UE_LOG(LogGravityBlueprintTools, Display, TEXT("Model Path: %s"), *modelPath);

	// Get model name
	FString modelName = FPaths::GetBaseFilename(modelPath);

	// Create blueprint path
	FString blueprintPath = FPaths::GetPath(modelPath) + "/" + modelName;

	FString blueprintName = FPaths::GetBaseFilename(blueprintPath);
	FString blueprintPackageName = FPackageName::GetLongPackagePath(blueprintPath) + "/" + blueprintName;

	// Check if blueprint already exists
	if (FPaths::FileExists(blueprintPath))
	{
		// UE_LOG(LogGravityBlueprintTools, Error, TEXT("Blueprint already exists at path: %s"), *blueprintPath);
		// return;

		// Delete existing blueprint
		UPackage *blueprintPackage = LoadPackage(nullptr, *blueprintPackageName, LOAD_None);
		if (blueprintPackage)
		{
			blueprintPackage->SetDirtyFlag(true);
			blueprintPackage->RemoveFromRoot();
			blueprintPackage->ConditionalBeginDestroy();
		}
	}

	// Create a new Blueprint Factory
	UBlueprintFactory *blueprintFactory = NewObject<UBlueprintFactory>();
	blueprintFactory->ParentClass = AActor::StaticClass(); // Set the parent class to AActor

	// Create blueprint Actor
	UBlueprint *blueprint = Cast<UBlueprint>(assetToolsModule.Get().CreateAsset(blueprintName, blueprintPath, UBlueprint::StaticClass(), blueprintFactory));

	if (!blueprint)
	{
		UE_LOG(LogGravityBlueprintTools, Error, TEXT("Failed to create blueprint at path: %s"), *blueprintPath);
		return;
	}

	// Get the Blueprint's generated class
	UClass *BlueprintClass = blueprint->GeneratedClass;
	if (!BlueprintClass)
	{
		UE_LOG(LogGravityBlueprintTools, Error, TEXT("Failed to get blueprint class"));
		return;
	}

}