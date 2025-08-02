#include "GravityMaterialPresetDatabase.h"

#include <Interfaces/IPluginManager.h>
#include <HAL/FileManagerGeneric.h>
#include <Serialization/JsonReader.h>
#include <Serialization/JsonSerializer.h>
#include <Dom/JsonValue.h>
#include <Dom/JsonObject.h>

DEFINE_LOG_CATEGORY(LogGravityMaterialDatabase)

TSharedPtr<FGravityMaterialPresetDatabase> FGravityMaterialPresetDatabase::Instance = nullptr;

FGravityMaterialPresetDatabase::FGravityMaterialPresetDatabase()
{
	DefaultMaterialPreset.Metallic = 0.3f;
	DefaultMaterialPreset.Roughness = 0.7f;
	DefaultMaterialPreset.Specular = 0.0f;
}

FString FGravityMaterialPresetDatabase::GetPresetTypeName(EGravityMaterialParameterPresetType PresetType) const
{
	switch (PresetType)
	{
	case EGravityMaterialParameterPresetType::Rock:		return TEXT("rock");	break;
	case EGravityMaterialParameterPresetType::Glass:	return TEXT("glass_1");	break;
	case EGravityMaterialParameterPresetType::Moss:		return TEXT("moss");	break;
	case EGravityMaterialParameterPresetType::Metal:	return TEXT("metal");	break;
	case EGravityMaterialParameterPresetType::Metal1:	return TEXT("metal_1");	break;
	case EGravityMaterialParameterPresetType::Soil:		return TEXT("soil");	break;
	case EGravityMaterialParameterPresetType::Wood:		return TEXT("wood");	break;
	case EGravityMaterialParameterPresetType::Cloth:	return TEXT("cloth");	break;
	case EGravityMaterialParameterPresetType::Grass:	return TEXT("grass");	break;
	case EGravityMaterialParameterPresetType::Vinyl:	return TEXT("vinyl");	break;
	};

	return TEXT("unknown");
};

EGravityMaterialParameterPresetType FGravityMaterialPresetDatabase::GetPresetType(const FString& PresetTypeName) const
{
	const FString lowerTypeName = PresetTypeName.ToLower();

	if (lowerTypeName == TEXT("rock"))   { return EGravityMaterialParameterPresetType::Rock;   }
	if (lowerTypeName == TEXT("glass_1"))  { return EGravityMaterialParameterPresetType::Glass;  }
	if (lowerTypeName == TEXT("moss"))   { return EGravityMaterialParameterPresetType::Moss;   }
	if (lowerTypeName == TEXT("metal"))  { return EGravityMaterialParameterPresetType::Metal;  }
	if (lowerTypeName == TEXT("metal_1")) { return EGravityMaterialParameterPresetType::Metal1; }
	if (lowerTypeName == TEXT("soil"))   { return EGravityMaterialParameterPresetType::Soil;   }
	if (lowerTypeName == TEXT("wood"))   { return EGravityMaterialParameterPresetType::Wood;   }
	if (lowerTypeName == TEXT("cloth"))  { return EGravityMaterialParameterPresetType::Cloth;  }
	if (lowerTypeName == TEXT("grass"))  { return EGravityMaterialParameterPresetType::Grass;  }
	if (lowerTypeName == TEXT("vinyl"))  { return EGravityMaterialParameterPresetType::Vinyl;  }

	return EGravityMaterialParameterPresetType::Unknown;
}

FGravityMaterialPresetDatabase& FGravityMaterialPresetDatabase::GetInstance()
{
	if (!Instance.IsValid())
	{
		Instance = MakeShared<FGravityMaterialPresetDatabase>();

		Instance->LoadMaterialDatabase();
	}

	return *Instance;
}

bool FGravityMaterialPresetDatabase::LoadMaterialDatabase()
{
	const FString pluginContentDir = IPluginManager::Get().FindPlugin("GravityTools")->GetContentDir();

	const FString materialDatabaseFile = FPaths::Combine(pluginContentDir, TEXT("MaterialDatabase"), TEXT("MaterialPresetDatabase.json"));

	static IFileManager& fileManager = FFileManagerGeneric::Get();

	if (!fileManager.FileExists(*materialDatabaseFile))
	{
		UE_LOG(LogGravityMaterialDatabase, Warning, TEXT("Cannot open the material preset database. Expected at '%s'."), *materialDatabaseFile);

		return false;
	}

	FString jsonAssetInfoString;

	if (!FFileHelper::LoadFileToString(jsonAssetInfoString, *materialDatabaseFile))
	{
		UE_LOG(LogGravityMaterialDatabase, Warning, TEXT("Could not load the material preset database '%s'."), *materialDatabaseFile);

		return false;
	}

	TSharedRef<TJsonReader<TCHAR>> jsonAssetInfoFileReader = TJsonReaderFactory<TCHAR>::Create(jsonAssetInfoString);

	TSharedPtr<FJsonObject> jsonAssetInfoRootObject;

	if (!FJsonSerializer::Deserialize(jsonAssetInfoFileReader, jsonAssetInfoRootObject))
	{
		UE_LOG(LogGravityMaterialDatabase, Warning, TEXT("Failed to parse the material preset database '%s'."), *materialDatabaseFile);

		return false;
	}

	auto ReadWorlLayerPresetsLambda = [this, &jsonAssetInfoRootObject](const TCHAR* WorldLayerName, TMap<EGravityMaterialParameterPresetType, FGravityMaterialParameterPresets>& OutMaterialPresets)
	{
		const TSharedPtr<FJsonObject>* layerPresetRoot = nullptr;

		if (!jsonAssetInfoRootObject->TryGetObjectField(WorldLayerName, layerPresetRoot))
		{
			UE_LOG(LogGravityMaterialDatabase, Warning, TEXT("Cannot read the presets for the primary world layer."));

			return false;
		}

		for (const auto& entry : (*layerPresetRoot)->Values)
		{
			const FString& presetTypeName = entry.Key;

			const TArray<TSharedPtr<FJsonValue>>* presetParameters = nullptr;

			if (!entry.Value->TryGetArray(presetParameters))
			{
				UE_LOG(LogGravityMaterialDatabase, Warning, TEXT("Cannot parse the material preset '%s'."), *presetTypeName);

				return false;
			}

			if (presetParameters->Num() != 3)
			{
				UE_LOG(LogGravityMaterialDatabase, Warning, TEXT("Unexpected number of preset parameters. Must be '%d' prameters."), 3);

				return false;
			}

			FGravityMaterialParameterPresets presets;

			presets.Roughness = static_cast<float>((*presetParameters)[0]->AsNumber());
			presets.Metallic = static_cast<float>((*presetParameters)[1]->AsNumber());
			presets.Specular = static_cast<float>((*presetParameters)[2]->AsNumber());

			EGravityMaterialWorldLayer worldLayer = GetWorldLayer(WorldLayerName);
			EGravityMaterialParameterPresetType presetType = GetPresetType(presetTypeName);

			if (worldLayer == EGravityMaterialWorldLayer::Unknown || presetType == EGravityMaterialParameterPresetType::Unknown)
			{
				UE_LOG(LogGravityMaterialDatabase, Warning, TEXT("The material database is corrupted or contains an invalid entry. Check '%s':'%s'"), WorldLayerName, *presetTypeName);

				return false;
			}

			OutMaterialPresets.Emplace(presetType, presets);
		}

		return true;
	};

	if (!ReadWorlLayerPresetsLambda(TEXT("G2_BG_Main"), PrimaryMaterialPresets))
	{
		return false;
	}

	if (!ReadWorlLayerPresetsLambda(TEXT("G2_BG_Layer"), SecondaryMaterialPresets))
	{
		return false;
	}

	// load preset for textures
	const TSharedPtr<FJsonObject>* texturePresetRoot = nullptr;

	if (!jsonAssetInfoRootObject->TryGetObjectField(TEXT("G2_TextureMapping"), texturePresetRoot))
	{
		UE_LOG(LogGravityMaterialDatabase, Warning, TEXT("Cannot load the texture presets."));

		return false;
	}

	for (const auto& entry : (*texturePresetRoot)->Values)
	{
		const FString& textureName = entry.Key;

		FString presetTypeName;

		if (!entry.Value->TryGetString(presetTypeName))
		{
			UE_LOG(LogGravityMaterialDatabase, Warning, TEXT("Cannot load preset for texture '%s'."), *textureName);

			return false;
		}

		EGravityMaterialParameterPresetType presetType = GetPresetType(presetTypeName);

		if (presetType == EGravityMaterialParameterPresetType::Unknown)
		{
			UE_LOG(LogGravityMaterialDatabase, Warning, TEXT("Cannot load preset '%s' for texture '%s'."), *presetTypeName, *textureName);

			return false;
		}

		TexturePresets.Emplace(textureName, presetType);
	}

	return true;
}

const EGravityMaterialWorldLayer FGravityMaterialPresetDatabase::GetWorldLayer(const FString& WorldLayerName) const
{
	const FString loweWorldLayerName = WorldLayerName.ToLower();

	if (loweWorldLayerName == TEXT("g2_bg_main")) return EGravityMaterialWorldLayer::Primary;
	if (loweWorldLayerName == TEXT("g2_bg_layer")) return EGravityMaterialWorldLayer::Secondary;

	return EGravityMaterialWorldLayer::Unknown;
}

const FString FGravityMaterialPresetDatabase::GetWorldLayerName(EGravityMaterialWorldLayer Layer) const
{
	switch (Layer)
	{
	case EGravityMaterialWorldLayer::Primary: return TEXT("G2_BG_Main");
	case EGravityMaterialWorldLayer::Secondary: return TEXT("G2_BG_Layer");
	}

	return TEXT("Unknown");
}

const FGravityMaterialParameterPresets& FGravityMaterialPresetDatabase::GetPreset(EGravityMaterialWorldLayer WorldLayer, EGravityMaterialParameterPresetType PresetType) const
{
	return WorldLayer == EGravityMaterialWorldLayer::Primary ? PrimaryMaterialPresets[PresetType] : SecondaryMaterialPresets[PresetType];
}

const FGravityMaterialParameterPresets& FGravityMaterialPresetDatabase::GetPreset(EGravityMaterialWorldLayer WorldLayer, const FString& PresetTypeName) const
{
	return GetPreset(WorldLayer, GetPresetType(PresetTypeName));
}

const FGravityMaterialParameterPresets& FGravityMaterialPresetDatabase::GetPreset(const FString& WorldLayerName, EGravityMaterialParameterPresetType PresetType) const
{
	return GetPreset(GetWorldLayer(WorldLayerName), PresetType);
}

const FGravityMaterialParameterPresets& FGravityMaterialPresetDatabase::GetPreset(const FString& WorldLayerName, const FString& PresetTypeName) const
{
	return GetPreset(GetWorldLayer(WorldLayerName), PresetTypeName);
}

const FGravityMaterialParameterPresets& FGravityMaterialPresetDatabase::GetPresetForTexture(EGravityMaterialWorldLayer WorldLayer, const FString& TextureName) const
{
	const EGravityMaterialParameterPresetType* preset = TexturePresets.Find(TextureName.ToLower());

	if (!preset)
	{
		return DefaultMaterialPreset;
	}

	return GetPreset(WorldLayer, *preset);
}

const FGravityMaterialParameterPresets& FGravityMaterialPresetDatabase::GetPresetForTexture(const FString& WorldLayerName, const FString& TextureName) const
{
	return GetPresetForTexture(GetWorldLayer(WorldLayerName), TextureName);
}