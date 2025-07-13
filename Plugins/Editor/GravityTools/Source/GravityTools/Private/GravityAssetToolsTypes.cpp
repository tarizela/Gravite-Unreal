#include "GravityAssetToolsTypes.h"

DEFINE_LOG_CATEGORY(LogGravityAssetTools)

bool IsGravityMaterialChannelNormalMap(EGravityMaterialType Type, int32 TextureChannel)
{
	switch (Type)
	{
	case EGravityMaterialType::Standard:
	{
		switch (TextureChannel)
		{
		case 1: // Normal 0
		case 4: // Normal 1
			return true;
		}
	}break;
	case EGravityMaterialType::Translucent:
	{
		// Has no normal maps.
		return false;
	}break;
	case EGravityMaterialType::TreeBranches:
	{
		// Has no normal maps.
		return false;
	}break;
	case EGravityMaterialType::Grass:
	{
		switch (TextureChannel)
		{
		case 1: // Normal 0
			return true;
		}
	}break;
	case EGravityMaterialType::GravityCrystal:
	{
		switch (TextureChannel)
		{
		case 1: // Normal 0
			return true;
		}
	}break;
	case EGravityMaterialType::TV:
	{
		// Has no normal maps.
		return false;
	}break;
	case EGravityMaterialType::Soil:
	{
		switch (TextureChannel)
		{
		case 4: // Normal 0
		case 5: // Normal 1
		case 6: // Normal 2
		case 7: // Normal 3
			return true;
		}
	}break;
	case EGravityMaterialType::Window:
	{
		switch (TextureChannel)
		{
		case 1: // Glass Normal
			return true;
		}
	}break;
	case EGravityMaterialType::Water:
	{
		switch (TextureChannel)
		{
		case 0: // Water Normal
			return true;
		}
	}break;
	case EGravityMaterialType::Glass:
	{
		switch (TextureChannel)
		{
		case 0: // Glass Normal
			return true;
		}
	}break;
	case EGravityMaterialType::Decal:
	{
		switch (TextureChannel)
		{
		case 1: // Normal 0
			return true;
		}
	}break;
	}

	return false;
}

bool DoesGravityMaterialSupportNanite(UMaterialInterface* Material)
{
	if (Material)
	{
		UMaterialInterface* baseMaterial = Material->GetBaseMaterial();
		if (!baseMaterial)
		{
			baseMaterial = Material;
		}

		const auto unsupportedMaterialTypes = 
		{ 
			EGravityMaterialType::Decal,
			EGravityMaterialType::Glass,
			EGravityMaterialType::Grass,
			EGravityMaterialType::Translucent,
			EGravityMaterialType::TreeBranches,
			EGravityMaterialType::Water
		};
		
		for (const auto& materialType : unsupportedMaterialTypes)
		{
			if (baseMaterial->GetName().EndsWith(GravityMaterialTypeToString(materialType)))
			{
				return false;
			}
		}
	}

	return true;
}

bool DoesStaticMeshSupportNanite(UStaticMesh* StaticMesh)
{
	if (StaticMesh)
	{
		int32 materialSlot = 0;
		while (UMaterialInterface* material = StaticMesh->GetMaterial(materialSlot++))
		{
			if (!DoesGravityMaterialSupportNanite(material))
			{
				return false;
			}
		}

		return true;
	}

	return false;
}

FString GravityMaterialTypeToString(EGravityMaterialType Type)
{
	switch (Type)
	{
	case EGravityMaterialType::Standard:		return TEXT("23");
	case EGravityMaterialType::Translucent:		return TEXT("24");
	case EGravityMaterialType::TreeBranches:	return TEXT("28");
	case EGravityMaterialType::Grass:			return TEXT("29");
	case EGravityMaterialType::GravityCrystal:	return TEXT("30");
	case EGravityMaterialType::TV:				return TEXT("2A");
	case EGravityMaterialType::Soil:			return TEXT("2B");
	case EGravityMaterialType::Window:			return TEXT("2C");
	case EGravityMaterialType::Water:			return TEXT("2D");
	case EGravityMaterialType::Glass:			return TEXT("2E");
	case EGravityMaterialType::Decal:			return TEXT("DC");
	}

	return TEXT("Unknown");
};

EGravityMaterialType StringToGravityMaterialType(const FString& TypeString)
{
	if (TypeString == TEXT("23")) { return EGravityMaterialType::Standard;		 }
	if (TypeString == TEXT("24")) { return EGravityMaterialType::Translucent;	 }
	if (TypeString == TEXT("28")) { return EGravityMaterialType::TreeBranches;	 }
	if (TypeString == TEXT("29")) { return EGravityMaterialType::Grass;			 }
	if (TypeString == TEXT("30")) { return EGravityMaterialType::GravityCrystal; }
	if (TypeString == TEXT("2A")) { return EGravityMaterialType::TV;			 }
	if (TypeString == TEXT("2B")) { return EGravityMaterialType::Soil;			 }
	if (TypeString == TEXT("2C")) { return EGravityMaterialType::Window;		 }
	if (TypeString == TEXT("2D")) { return EGravityMaterialType::Water;			 }
	if (TypeString == TEXT("2E")) { return EGravityMaterialType::Glass;			 }
	if (TypeString == TEXT("DC")) { return EGravityMaterialType::Decal;			 }

	return EGravityMaterialType::Unknown;
}

FString GravityMaterialParameterTypeToString(EGravityMaterialParameterType Type)
{
	switch (Type)
	{
	case EGravityMaterialParameterType::PresetType:			return TEXT("PresetType");
	case EGravityMaterialParameterType::Metallic0:			return TEXT("Metallic0");
	case EGravityMaterialParameterType::Roughness0:			return TEXT("Roughness0");
	case EGravityMaterialParameterType::Specular0:			return TEXT("Specular0");
	case EGravityMaterialParameterType::EmissionBoost0:		return TEXT("EmissionBoost0");
	case EGravityMaterialParameterType::ParallaxHeight0:	return TEXT("ParallaxHeight0");
	case EGravityMaterialParameterType::Metallic1:			return TEXT("Metallic1");
	case EGravityMaterialParameterType::Roughness1:			return TEXT("Roughness1");
	case EGravityMaterialParameterType::Specular1:			return TEXT("Specular1");
	case EGravityMaterialParameterType::EmissionBoost1:		return TEXT("EmissionBoost1");
	case EGravityMaterialParameterType::ParallaxHeight1:	return TEXT("ParallaxHeight1");
	case EGravityMaterialParameterType::ColorR0:			return TEXT("ColorR0");
	case EGravityMaterialParameterType::ColorG0:			return TEXT("ColorG0");
	case EGravityMaterialParameterType::ColorB0:			return TEXT("ColorB0");
	case EGravityMaterialParameterType::ColorR1:			return TEXT("ColorR1");
	case EGravityMaterialParameterType::ColorG1:			return TEXT("ColorG1");
	case EGravityMaterialParameterType::ColorB1:			return TEXT("ColorB1");
	case EGravityMaterialParameterType::AlphaTest:			return TEXT("AlphaTest");
	}

	return "Unknown";
}

FString GravityMaterialChannelTypeToString(int32 Channel)
{
	if (Channel >= 0)
	{
		return FString::Format(TEXT("Texture{0}"), { Channel });
	}

	return "Unknown";
}

const FGravityMaterialParameter* FGravityMaterialInfo::GetParameter(EGravityMaterialParameterType ParameterType) const
{
	const FGravityMaterialParameterPtr* foundParameter = Parameters.Find(ParameterType);

	if (foundParameter)
	{
		return foundParameter->Get();
	}

	return nullptr;
}

void FGravityMaterialInfo::SetParameter(EGravityMaterialParameterType ParameterType, const FGravityMaterialParameterPtr& Parameter)
{
	// only store known paramters
	if (ParameterType != EGravityMaterialParameterType::Unknown)
	{
		Parameters.FindOrAdd(ParameterType) = Parameter;
	}
}

float FGravityMaterialInfo::SafeGetFloatParameter(EGravityMaterialParameterType ParameterType, float DefaultValue) const
{
	const FGravityMaterialParameter* parameter = GetParameter(ParameterType);

	if (!parameter)
	{
		UE_LOG(LogGravityAssetTools, Warning, TEXT("The material '%s' does not have the parameter '%s'."), *Name, *GravityMaterialParameterTypeToString(ParameterType));

		return DefaultValue;
	}

	if (!parameter->Is<float>())
	{
		UE_LOG(LogGravityAssetTools, Warning, TEXT("The accessed parameter of material '%s' is not a float. Using the default value."), *Name);

		return DefaultValue;
	}

	return parameter->As<float>();
}

FString FGravityMaterialInfo::SafeGetStringParameter(EGravityMaterialParameterType ParameterType, const TCHAR* DefaultValue) const
{
	const FGravityMaterialParameter* parameter = GetParameter(ParameterType);

	if (!parameter)
	{
		UE_LOG(LogGravityAssetTools, Warning, TEXT("The material '%s' does not have the parameter '%s'."), *Name, *GravityMaterialParameterTypeToString(ParameterType));

		return DefaultValue;
	}

	if (!parameter->Is<FString>())
	{
		UE_LOG(LogGravityAssetTools, Warning, TEXT("The accessed parameter of material '%s' is not a string. Using default value."), *Name);

		return DefaultValue;
	}

	return parameter->As<FString>();
}

const FGravityMaterialTextureInfo* FGravityMaterialInfo::GetTextureInfo(int32 Channel) const
{
	if (Channel >= 0)
	{
		return Textures.Find(Channel);
	}

	return nullptr;
}

bool FGravityMaterialInfo::SetTextureInfo(const FGravityMaterialTextureInfo& Info)
{
	if (Info.TextureChannel >= 0)
	{
		Textures.Emplace(Info.TextureChannel, Info);

		return true;
	}

	return false;
}