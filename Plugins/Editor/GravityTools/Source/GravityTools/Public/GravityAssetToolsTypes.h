#pragma once

#include <CoreMinimal.h>

DECLARE_LOG_CATEGORY_EXTERN(LogGravityAssetTools, Display, All)

class FGravityMaterialParameter
{
public:
	virtual ~FGravityMaterialParameter() = default;

	template<class T> const bool Is() const { return false; }
	template<> const bool Is<float>() const { return IsFloat(); }
	template<> const bool Is<FString>() const { return IsString(); }

	template<class T> const T& As() const { return *reinterpret_cast<T*>(nullptr); }
	template<> const float& As<float>() const { return *AsFloat(); }
	template<> const FString& As<FString>() const { return *AsString(); }

protected:
	virtual const bool IsFloat() const { return false; }
	virtual const bool IsString() const { return false; }

	virtual const float* AsFloat() const { return nullptr; }
	virtual const FString* AsString() const { return nullptr; }
};

using FGravityMaterialParameterPtr = TSharedPtr<FGravityMaterialParameter>;

class FGravityMaterialParameterFloat final : public FGravityMaterialParameter
{
public:
	FGravityMaterialParameterFloat(float InValue)
		: Value(InValue)
	{
	}

protected:
	const bool IsFloat() const override { return true; }

	const float* AsFloat() const override { return &Value; }

private:
	float Value = 0.0f;
};

class FGravityMaterialParameterString final : public FGravityMaterialParameter
{
public:
	FGravityMaterialParameterString(const FString& InValue)
		: Value(InValue)
	{
	}

protected:
	const bool IsString() const override { return true; }

	const FString* AsString() const override { return &Value; }

private:
	FString Value;
};

enum class EGravityMaterialType : uint32
{
	Unknown,

	Standard,		// 23
	Translucent,	// 24
	TreeBranches,	// 28
	Grass,			// 29
	GravityCrystal,	// 30
	TV,				// 2A
	Soil,			// 2B
	Window,			// 2C
	Water,			// 2D
	Glass,			// 2E
	Decal			// DC
};

enum class EGravityMaterialParameterType : int32
{
	Unknown,

	PresetType,

	Roughness0,
	Metallic0,
	Specular0,
	EmissionBoost0,
	ParallaxHeight0,
	ColorR0,
	ColorG0,
	ColorB0,

	Roughness1,
	Metallic1,
	Specular1,
	EmissionBoost1,
	ParallaxHeight1,
	ColorR1,
	ColorG1,
	ColorB1,

	Roughness2,
	Metallic2,
	Specular2,
	EmissionBoost2,
	ParallaxHeight2,
	ColorR2,
	ColorG2,
	ColorB2,

	Roughness3,
	Metallic3,
	Specular3,
	EmissionBoost3,
	ParallaxHeight3,
	ColorR3,
	ColorG3,
	ColorB3,

	AlphaTest,
	IOR,
	Opacity,

	PositionOffsetX,
	PositionOffsetY,
	PositionOffsetZ,
	BoundingSphereRadius
};

ENUM_RANGE_BY_FIRST_AND_LAST(EGravityMaterialParameterType, EGravityMaterialParameterType::PresetType, EGravityMaterialParameterType::BoundingSphereRadius);

enum class EGravityMaterialFlags : uint8
{
	None = 0,				// the material is two sided

	AlphaTested = 0x2,		// the material uses alpha testing

	UseFaceCulling = 0x4,	// the material uses face culling

	Unknown_3 = 0x8			// need to figure it out later (shadow casting maybe?)
};

ENUM_CLASS_FLAGS(EGravityMaterialFlags);

class FGravityMaterialTextureInfo
{
public:
	/** The name of the texture. */
	FString Name;

	/** The material texture channel. */
	int32 TextureChannel = -1;

	/** Is set to true if this texture will be assigned to a normal map channel. */
	bool bIsNormalMap = false;

	/** Texture address mode to use. */
	TEnumAsByte<TextureAddress> AddressX = TextureAddress::TA_Wrap;
	TEnumAsByte<TextureAddress> AddressY = TextureAddress::TA_Wrap;

	/** UV offset. */
	FVector2D UVOffset = FVector2D::ZeroVector;

	/** UV scale. */
	FVector2D UVScale = FVector2D::ZeroVector;

	/** Misc parameters. */
	TStaticArray<float, 3> Parameters;
};

class FGravityMaterialInfo
{
public:
	const FGravityMaterialParameter* GetParameter(EGravityMaterialParameterType ParameterType) const;
	void SetParameter(EGravityMaterialParameterType ParameterType, const FGravityMaterialParameterPtr& Parameter);

	float SafeGetFloatParameter(EGravityMaterialParameterType ParameterType, float DefaultValue) const;
	FString SafeGetStringParameter(EGravityMaterialParameterType ParameterType, const TCHAR* DefaultValue) const;

	const FGravityMaterialTextureInfo* GetTextureInfo(int32 Channel) const;
	bool SetTextureInfo(const FGravityMaterialTextureInfo& Info);

	const TMap<int32, FGravityMaterialTextureInfo>& GetTextureInfos() const { return Textures; }

public:
	/** the type of the material 01, 30, 2A, ... */
	EGravityMaterialType Type = EGravityMaterialType::Unknown;

	/** the material flags */
	EGravityMaterialFlags Flags;

	/** the material type string */
	FString TypeString;

	/** name of the material */
	FString Name;

	/** if the material can be shared with other assets */
	bool bIsShareable = false;

private:
	/** various parameters of the material */
	TMap<EGravityMaterialParameterType, FGravityMaterialParameterPtr> Parameters;

	/** texture and texture atlas slots */
	TMap<int32, FGravityMaterialTextureInfo> Textures;
};

class FGravityAssetInfo
{
public:
	// the name of the loaded asset
	FString Name;

	// the source (fbx) file path of the model
	FString SourceFilePath;

	// names of the model meshes
	TArray<FString> MeshNames;

	// the material of the model
	TMap<FString, FGravityMaterialInfo> MaterialInfos;
};

/**
 * @returns True if the channel type is intended for normal maps.
 */
bool IsGravityMaterialChannelNormalMap(EGravityMaterialType Type, int32 TextureChannel);

/**
 * @returns True if the mesh supports nanite.
 */
bool DoesStaticMeshSupportNanite(UStaticMesh* StaticMesh);

/**
 * @returns True if the material supports nanite.
 */
bool DoesGravityMaterialSupportNanite(UMaterialInterface* Material);

/**
 * @returns The parameter type as string.
 */
FString GravityMaterialParameterTypeToString(EGravityMaterialParameterType Type);

/**
 * @returns The parsed material type.
 */
EGravityMaterialType StringToGravityMaterialType(const FString& TypeString);

/**
 * @returns The material type as string.
 */
FString GravityMaterialTypeToString(EGravityMaterialType Type);

using FGravityAssetInfoPtr = TSharedPtr<FGravityAssetInfo>;