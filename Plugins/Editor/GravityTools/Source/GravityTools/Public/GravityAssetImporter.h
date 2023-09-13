#pragma once

#include <CoreMinimal.h>
#include <Widgets/SCompoundWidget.h>
#include <Misc/NotifyHook.h>

DECLARE_LOG_CATEGORY_EXTERN(LogGravityAssetImporter, Display, All)

class IDetailsView;
class UStaticMesh;
class UTexture;
class UMaterialInstanceConstant;
class IAssetTools;
class IAssetRegistry;
class UGravityAssetImporterArguments;
class SGravityAssetImporterListView;
class FGravityAssetImporterMaterialParameterFloat;
class FGravityAssetImporterMaterialParameterString;

class FGravityAssetImporterMaterialParameter
{
public:
	virtual ~FGravityAssetImporterMaterialParameter() = default;

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

using FGravityAssetImporterMaterialParameterPtr = TSharedPtr<FGravityAssetImporterMaterialParameter>;

class FGravityAssetImporterMaterialParameterFloat final : public FGravityAssetImporterMaterialParameter
{
public:
	FGravityAssetImporterMaterialParameterFloat(float InValue)
		: Value(InValue)
	{
	}

protected:
	const bool IsFloat() const override { return true; }

	const float* AsFloat() const override { return &Value; }

private:
	float Value = 0.0f;
};

class FGravityAssetImporterMaterialParameterString final : public FGravityAssetImporterMaterialParameter
{
public:
	FGravityAssetImporterMaterialParameterString(const FString& InValue)
		: Value(InValue)
	{
	}

protected:
	const bool IsString() const override { return true; }

	const FString* AsString() const override { return &Value; }

private:
	FString Value;
};

class FGravityAssetImporterTextureInfo
{
public:
	FString Name;

	FVector2D UVOffset = FVector2D::ZeroVector;
	FVector2D UVScale = FVector2D::ZeroVector;

	TStaticArray<float, 3> Parameters;
};

class FGravityAssetImporterMaterialInfo
{
public:
	/** the type of the material 01, 30, 2A, ... */
	FString Type;

	/** name of the material */
	FString Name;

	/** various parameters of the material */
	TArray<FGravityAssetImporterMaterialParameterPtr> Parameters;

	/** texture and texture atlas slots */
	TMap<int32, FGravityAssetImporterTextureInfo> Textures;
};

class FGravityAssetImporterAssetInfo
{
public:
	// the name of the loaded asset
	FString Name;

	// the name of the model meshes
	TArray<FString> ModelNames;

	// the name of the model collider
	FString ColliderName;

	// the material of the model
	TMap<FString, FGravityAssetImporterMaterialInfo> MaterialInfos;
};

using FGravityAssetImporterAssetInfoPtr = TSharedPtr<FGravityAssetImporterAssetInfo>;

class SGravityAssetImporter : public SCompoundWidget, public FNotifyHook
{
	SLATE_BEGIN_ARGS(SGravityAssetImporter)
	{
	}
	SLATE_END_ARGS()

public:
	SGravityAssetImporter();
	~SGravityAssetImporter();

	void Construct(const FArguments& Args);

private:
	void RebuildAssetListView();
	FReply OnImportClicked();

	virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged) override;

	void ImportMeshes();
	UTexture* ImportTexture(const FString& TextureFilePath);
	void ModifyImportedStaticMesh(UStaticMesh* StaticMesh);
	void CreateMaterials(UStaticMesh* StaticMesh, const TMap<FString, FGravityAssetImporterMaterialInfo>& MaterialInfos, const FString& MaterialInstancePackageDir);
	UMaterialInstanceConstant* GetOrCreateMaterialInstance(const FGravityAssetImporterMaterialInfo& MaterialInfo, const FString& MaterialInstancePackageDir);

private:
	TSharedPtr<IDetailsView> ArgumentsDetailsView;
	TSharedPtr<SGravityAssetImporterListView> AssetListView;

	TStrongObjectPtr<UGravityAssetImporterArguments> Arguments;

	IAssetTools* AssetTools = nullptr;

	IAssetRegistry* AssetRegistry = nullptr;

	/** A registry of created materials */
	TMap<FString, FString> MaterialRegistry;
};