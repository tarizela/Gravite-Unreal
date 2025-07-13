#pragma once

#include "GravityAssetToolsTypes.h"

struct FGravityMaterialRegistryItem
{
	FString MaterialAssetName;

	/** Will be empty if the material asset does not exist. */
	FString MaterialAssetReference;

	FGravityMaterialInfo MaterialInfo;
};

class FGravityMaterialRegistry
{
public:
	/**
	 * Registers the materials of an assets.
	 */
	void RegisterAssetMaterials(const FGravityAssetInfo& GravityAssetInfos);

	FGravityMaterialRegistryItem* FindMaterial(const FGravityMaterialInfo& MaterialInfo);

	void Reset();

private:
	TArray<FGravityMaterialRegistryItem> RegisteredMaterials;

	TMap<const FString, TArray<int32>> RegisteredMaterialsViewByName;

	TMap<EGravityMaterialType, TArray<int32>> RegisteredMaterialsViewByType;
};