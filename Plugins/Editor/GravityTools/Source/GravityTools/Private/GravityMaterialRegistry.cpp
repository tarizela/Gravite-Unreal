#include "GravityMaterialRegistry.h"

#include <ObjectTools.h>

static bool operator==(const FGravityMaterialTextureInfo& Rhs, const FGravityMaterialTextureInfo& Lhs)
{
	if (Lhs.Name != Rhs.Name)								{ return false; }
	if (Lhs.TextureChannel != Rhs.TextureChannel)			{ return false; }
	if (Lhs.bIsNormalMap != Rhs.bIsNormalMap)				{ return false; }
	if (Lhs.UVOffset != Rhs.UVOffset)						{ return false; }
	if (Lhs.UVScale != Rhs.UVScale)							{ return false; }

	// parameters are not compared because they are currently not being used for the UE materials

	return true;
}

static bool operator!=(const FGravityMaterialTextureInfo& Rhs, const FGravityMaterialTextureInfo& Lhs)
{
	return !(Rhs == Lhs);
}

static bool operator==(const FGravityMaterialInfo& Lhs, const FGravityMaterialInfo& Rhs)
{
	// we do not check the name of the material because it can be different even when the material parameters are the same

	if (Lhs.bIsShareable != Rhs.bIsShareable)	{ return false; }
	if (Lhs.Type != Rhs.Type)					{ return false; }
	if (Lhs.TypeString != Rhs.TypeString)		{ return false; }
	if (Lhs.Flags != Rhs.Flags)					{ return false; }

	auto IsMaterialParameterSameLambda = [&Lhs, &Rhs](EGravityMaterialParameterType ParameterType)
	{
		const auto* parameterLhs = Lhs.GetParameter(ParameterType);
		const auto* parameterRhs = Rhs.GetParameter(ParameterType);

		if (!parameterLhs && !parameterRhs)
		{
			// the materials do not have this parameter
			return true;
		}
		else if (!parameterLhs || !parameterRhs)
		{
			return false;
		}

		if (parameterLhs->Is<float>() != parameterRhs->Is<float>() || parameterLhs->Is<FString>() != parameterRhs->Is<FString>())
		{
			return false;
		}

		if (parameterLhs->Is<float>())
		{
			if (parameterLhs->As<float>() != parameterRhs->As<float>())
			{
				return false;
			}
		}
		else if (parameterLhs->Is<FString>())
		{
			if (parameterLhs->As<FString>() != parameterRhs->As<FString>())
			{
				return false;
			}
		}
		else
		{
			return false;
		}

		return true;
	};

	for (EGravityMaterialParameterType parameter : TEnumRange<EGravityMaterialParameterType>())
	{
		if (!IsMaterialParameterSameLambda(parameter))
		{
			return false;
		}
	}

	const auto& textureInfosLhs = Lhs.GetTextureInfos();
	const auto& textureInfosRhs = Rhs.GetTextureInfos();
	
	if (textureInfosLhs.Num() != textureInfosRhs.Num())
	{
		return false;
	}

	for (int32 channelIndex = 0; channelIndex < textureInfosLhs.Num(); ++channelIndex)
	{
		const auto* textureInfoLhs = Lhs.GetTextureInfo(channelIndex);
		const auto* textureInfoRhs = Rhs.GetTextureInfo(channelIndex);

		if (!textureInfoLhs && !textureInfoRhs)
		{
			// the material does not have this texture slot
			return true;
		}
		else if (!textureInfoLhs || !textureInfoRhs)
		{
			return false;
		}

		if (*textureInfoLhs != *textureInfoRhs)
		{
			return false;
		}
	};

	return true;
}

void FGravityMaterialRegistry::Reset()
{
	RegisteredMaterials.Empty();
	RegisteredMaterialsViewByName.Empty();
	RegisteredMaterialsViewByType.Empty();
}

FGravityMaterialRegistryItem* FGravityMaterialRegistry::FindMaterial(const FGravityMaterialInfo& MaterialInfo)
{
	FString materialName = MaterialInfo.Name;

	TArray<int32>* materialRegistryItemsViewByName = RegisteredMaterialsViewByName.Find(materialName);

	if (materialRegistryItemsViewByName)
	{
		for (int32 materialRegistryItemIdx : *materialRegistryItemsViewByName)
		{
			FGravityMaterialRegistryItem& materialRegistryItem = RegisteredMaterials[materialRegistryItemIdx];

			if (materialRegistryItem.MaterialInfo == MaterialInfo)
			{
				return &materialRegistryItem;
			}
		}
	}

	TArray<int32>* materialRegistryItemsViewByType = RegisteredMaterialsViewByType.Find(MaterialInfo.Type);

	if (materialRegistryItemsViewByType)
	{
		if (materialRegistryItemsViewByType)
		{
			for (int32 materialRegistryItemIdx : *materialRegistryItemsViewByType)
			{
				FGravityMaterialRegistryItem& materialRegistryItem = RegisteredMaterials[materialRegistryItemIdx];

				if (materialRegistryItem.MaterialInfo == MaterialInfo)
				{
					return &materialRegistryItem;
				}
			}
		}
	}

	return nullptr;
}

void FGravityMaterialRegistry::RegisterAssetMaterials(const FGravityAssetInfo& GravityAssetInfos)
{
	for (const auto& entry : GravityAssetInfos.MaterialInfos)
	{
		FString materialName = entry.Key;
		const FGravityMaterialInfo& materialInfo = entry.Value;

		TArray<int32>* materialRegistryItemsViewByName = RegisteredMaterialsViewByName.Find(materialName);

		bool bIsMaterialRegistered = false;

		// first try to find the material by name
		if (materialRegistryItemsViewByName)
		{
			for (int32 materialRegistryItemIdx : *materialRegistryItemsViewByName)
			{
				const FGravityMaterialRegistryItem& materialRegistryItem = RegisteredMaterials[materialRegistryItemIdx];

				if (materialRegistryItem.MaterialInfo == materialInfo)
				{
					// the material info is registered
					bIsMaterialRegistered = true;

					break;
				}
			}
		}
		
		// now try to find the material by parameters (linear search)
		TArray<int32>* materialRegistryItemsViewByType = RegisteredMaterialsViewByType.Find(materialInfo.Type);

		if (!bIsMaterialRegistered && materialRegistryItemsViewByType)
		{
			for (int32 materialRegistryItemIdx : *materialRegistryItemsViewByType)
			{
				const FGravityMaterialRegistryItem& materialRegistryItem = RegisteredMaterials[materialRegistryItemIdx];

				if (materialRegistryItem.MaterialInfo == materialInfo)
				{
					// the material info is registered
					bIsMaterialRegistered = true;

					break;
				}
			}
		}

		if (!bIsMaterialRegistered)
		{
			materialRegistryItemsViewByName = &RegisteredMaterialsViewByName.FindOrAdd(materialName);
			materialRegistryItemsViewByType = &RegisteredMaterialsViewByType.FindOrAdd(materialInfo.Type);

			// register the material
			FGravityMaterialRegistryItem materialRegistryItem;
			materialRegistryItem.MaterialAssetName = FString::Printf(TEXT("%s_%d"), *ObjectTools::SanitizeObjectName(materialName), materialRegistryItemsViewByName->Num());
			materialRegistryItem.MaterialInfo = materialInfo;

			int32 materialRegistryItemIdx = RegisteredMaterials.Emplace(MoveTemp(materialRegistryItem));

			// by name
			materialRegistryItemsViewByName->Add(materialRegistryItemIdx);

			// by type
			materialRegistryItemsViewByType->Add(materialRegistryItemIdx);
		}
	}
}