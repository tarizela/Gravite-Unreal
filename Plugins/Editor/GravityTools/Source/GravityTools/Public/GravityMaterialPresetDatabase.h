#pragma once

#include <CoreMinimal.h>

DECLARE_LOG_CATEGORY_EXTERN(LogGravityMaterialDatabase, Display, All)

enum class EGravityMaterialParameterPresetType : int32
{
	Unknown,

	Rock,
	Glass,
	Moss,
	Metal,
	Metal1,
	Soil,
	Wood,
	Cloth,
	Grass,
	Vinyl
};

enum class EGravityMaterialWorldLayer : int32
{
	Unknown,

	Primary,
	Secondary
};

struct FGravityMaterialParameterPresets
{
	float Roughness = 0.0f;
	float Metallic = 0.0f;
	float Specular = 0.0f;
};

class FGravityMaterialPresetDatabase
{
public:
	FGravityMaterialPresetDatabase();

	const FGravityMaterialParameterPresets& GetPreset(EGravityMaterialWorldLayer WorldLayer, EGravityMaterialParameterPresetType PresetType) const;
	const FGravityMaterialParameterPresets& GetPreset(const FString& WorldLayerName, EGravityMaterialParameterPresetType PresetType) const;

	const FGravityMaterialParameterPresets& GetPreset(EGravityMaterialWorldLayer WorldLayer, const FString& PresetTypeName) const;
	const FGravityMaterialParameterPresets& GetPreset(const FString& WorldLayerName, const FString& PresetTypeName) const;

	const FGravityMaterialParameterPresets& GetPresetForTexture(EGravityMaterialWorldLayer WorldLayer, const FString& TextureName) const;
	const FGravityMaterialParameterPresets& GetPresetForTexture(const FString& WorldLayerName, const FString& TextureName) const;

	const EGravityMaterialWorldLayer GetWorldLayer(const FString& WorldLayerName) const;
	const FString GetWorldLayerName(EGravityMaterialWorldLayer Layer) const;

	EGravityMaterialParameterPresetType GetPresetType(const FString& PresetTypeName) const;
	FString GetPresetTypeName(EGravityMaterialParameterPresetType PresetType) const;
	
	static FGravityMaterialPresetDatabase& GetInstance();

private:
	bool LoadMaterialDatabase();

private:
	TMap<EGravityMaterialParameterPresetType, FGravityMaterialParameterPresets> PrimaryMaterialPresets;
	TMap<EGravityMaterialParameterPresetType, FGravityMaterialParameterPresets> SecondaryMaterialPresets;
	TMap<const FString, EGravityMaterialParameterPresetType> TexturePresets;

	FGravityMaterialParameterPresets DefaultMaterialPreset;

private:
	static TSharedPtr<FGravityMaterialPresetDatabase> Instance;
};