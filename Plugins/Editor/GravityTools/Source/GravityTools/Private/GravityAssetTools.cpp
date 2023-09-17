#include "GravityAssetTools.h"

#include <Widgets/Input/SButton.h>
#include <Widgets/Layout/SScrollBox.h>
#include <Widgets/Layout/SSplitter.h>
#include <PropertyEditorModule.h>
#include <DetailsViewArgs.h>
#include <HAL/FileManagerGeneric.h>
#include <HAL/IConsoleManager.h>
#include <Serialization/JsonReader.h>
#include <Serialization/JsonSerializer.h>
#include <Dom/JsonValue.h>
#include <Dom/JsonObject.h>
#include <Misc/DefaultValueHelper.h>
#include <Misc/ScopedSlowTask.h>
#include <Misc/FileHelper.h>
#include <FileHelpers.h>
#include <AssetToolsModule.h>
#include <AssetRegistry/AssetRegistryModule.h>
#include <Modules/ModuleManager.h>
#include <Factories/FbxFactory.h>
#include <Factories/FbxImportUI.h>
#include <Factories/FbxStaticMeshImportData.h>
#include <Factories/MaterialInstanceConstantFactoryNew.h>
#include <Factories/TextureFactory.h>
#include <Engine/StaticMesh.h>
#include <AssetImportTask.h>
#include <Materials/MaterialInstanceConstant.h>

DEFINE_LOG_CATEGORY(LogGravityAssetTools)

#define LOCTEXT_NAMESPACE "UGravityAssetTools"

namespace GravityAssetInfoID
{
	static const TCHAR* ModelNames = TEXT("ModelNames");
	static const TCHAR* ColliderName = TEXT("ColliderName");
	static const TCHAR* Materials = TEXT("Materials");
	static const TCHAR* MaterialType = TEXT("Type");
	static const TCHAR* MaterialParameters = TEXT("Parameters");
	static const TCHAR* MaterialTextures = TEXT("Textures");
	static const TCHAR* MaterialTextureName = TEXT("Name");
	static const TCHAR* MaterialTextureParameters = TEXT("Parameters");
	static const TCHAR* MaterialTextureAtlasParameters = TEXT("Atlas");
};

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

class FGravityTextureInfo
{
public:
	FString Name;

	FVector2D UVOffset = FVector2D::ZeroVector;
	FVector2D UVScale = FVector2D::ZeroVector;

	TStaticArray<float, 3> Parameters;
};

class FGravityMaterialInfo
{
public:
	/** the type of the material 01, 30, 2A, ... */
	FString Type;

	/** name of the material */
	FString Name;

	/** various parameters of the material */
	TArray<FGravityMaterialParameterPtr> Parameters;

	/** texture and texture atlas slots */
	TMap<int32, FGravityTextureInfo> Textures;
};

class FGravityAssetInfo
{
public:
	// the name of the loaded asset
	FString Name;

	// the source (fbx) file path of the model
	FString SourceFilePath;

	// the name of the model meshes
	TArray<FString> ModelNames;

	// the name of the model collider
	FString ColliderName;

	// the material of the model
	TMap<FString, FGravityMaterialInfo> MaterialInfos;
};

using FGravityAssetInfoPtr = TSharedPtr<FGravityAssetInfo>;

static bool ParseMaterialParameters(const TArray<TSharedPtr<FJsonValue>>& ParameterValues, const FString& MaterialName, FGravityMaterialInfo& OutMaterialInfo)
{
	TArray<FGravityMaterialParameterPtr> materialParameters;

	for (int32 i = 0; i < ParameterValues.Num(); ++i)
	{
		const auto& jsonMaterialParameter = ParameterValues[i];

		switch (jsonMaterialParameter->Type)
		{
		case EJson::Number:	materialParameters.Emplace(MakeShared<FGravityMaterialParameterFloat>(jsonMaterialParameter->AsNumber())); break;
		case EJson::String:	materialParameters.Emplace(MakeShared<FGravityMaterialParameterString>(jsonMaterialParameter->AsString())); break;
		{
		}break;
		default:
			UE_LOG(LogGravityAssetTools, Warning, TEXT(""), *MaterialName, i);
			return false;
		}
	}

	OutMaterialInfo.Parameters = MoveTemp(materialParameters);

	return true;
}

static FGravityAssetInfoPtr LoadGravityAssetInfoFromDirectory(const FString& AssetDirectory)
{
	static IFileManager& fileManager = FFileManagerGeneric::Get();

	// the asset directory must contain a json file with the same name as the directory with the info of the asset.
	FString assetName = FPaths::GetBaseFilename(AssetDirectory);

	FString assetInfoFilePath = FPaths::Combine(AssetDirectory, assetName + ".json");
	FString assetSourceFilePath = FPaths::Combine(AssetDirectory, assetName + ".fbx");

	if (!fileManager.FileExists(*assetSourceFilePath))
	{
		UE_LOG(LogGravityAssetTools, Warning, TEXT("The asset directory '%s' is missing the '%s.fbx' file."), *AssetDirectory, *assetName);

		return nullptr;
	}

	FString jsonAssetInfoString;

	if (!FFileHelper::LoadFileToString(jsonAssetInfoString, *assetInfoFilePath))
	{
		UE_LOG(LogGravityAssetTools, Warning, TEXT("The asset directory '%s' must contain a valid '%s.json' file."), *AssetDirectory, *assetName);

		return nullptr;
	}

	TSharedRef<TJsonReader<TCHAR>> jsonAssetInfoFileReader = TJsonReaderFactory<TCHAR>::Create(jsonAssetInfoString);

	TSharedPtr<FJsonObject> jsonAssetInfoRootObject;

	if (!FJsonSerializer::Deserialize(jsonAssetInfoFileReader, jsonAssetInfoRootObject))
	{
		UE_LOG(LogGravityAssetTools, Warning, TEXT("Failed to parse: '%s'."), *assetInfoFilePath);

		return nullptr;
	}

	TArray<FString> assetModelNames;

	const auto WarnFailedToParseFieldLambda = [&assetInfoFilePath](const TCHAR* ParsedFiledName)
	{
		UE_LOG(LogGravityAssetTools, Warning, TEXT("Failed to parse: '%s'. The info file must contain the filed '%s'."), *assetInfoFilePath, ParsedFiledName);
	};

	const auto WarnFailedToParseMaterialFieldLambda = [](const TCHAR* MaterialName, const TCHAR* ParsedFiledName)
	{
		UE_LOG(LogGravityAssetTools, Warning, TEXT("Failed to parse material '%s'. The material must contain the filed '%s'."), MaterialName, ParsedFiledName);
	};

	const auto WarnFailedToParseMaterialTextureFieldLambda = [](const TCHAR* MaterialName, const TCHAR* TextureName)
	{
		UE_LOG(LogGravityAssetTools, Warning, TEXT("The material '%s' has an illformated texture parameter '%s'."), MaterialName, TextureName);
	};

	if (!jsonAssetInfoRootObject->TryGetStringArrayField(GravityAssetInfoID::ModelNames, assetModelNames))
	{
		WarnFailedToParseFieldLambda(GravityAssetInfoID::ModelNames);

		return nullptr;
	}

	FString assetColliderName;

	if (!jsonAssetInfoRootObject->TryGetStringField(GravityAssetInfoID::ColliderName, assetColliderName))
	{
		WarnFailedToParseFieldLambda(GravityAssetInfoID::ColliderName);

		return nullptr;
	}

	const TSharedPtr<FJsonObject>* jsonAssetMaterialsObject = nullptr;

	if (!jsonAssetInfoRootObject->TryGetObjectField(GravityAssetInfoID::Materials, jsonAssetMaterialsObject))
	{
		WarnFailedToParseFieldLambda(GravityAssetInfoID::Materials);

		return nullptr;
	}

	FGravityAssetInfoPtr gravityAssetInfo = MakeShared<FGravityAssetInfo>();

	gravityAssetInfo->Name = assetName;
	gravityAssetInfo->ModelNames = assetModelNames;
	gravityAssetInfo->ColliderName = assetColliderName;
	gravityAssetInfo->SourceFilePath = assetSourceFilePath;

	for (const auto& materialObjectEntry : (*jsonAssetMaterialsObject)->Values)
	{
		FGravityMaterialInfo materialInfo;

		materialInfo.Name = materialObjectEntry.Key;

		if (materialInfo.Name.IsEmpty())
		{
			UE_LOG(LogGravityAssetTools, Warning, TEXT("The asset '%s' has a material with no name. The material will not be processed."), *gravityAssetInfo->Name);

			return nullptr;
		}

		const TSharedPtr<FJsonObject>& jsonMaterialObject = materialObjectEntry.Value->AsObject();

		const TArray<TSharedPtr<FJsonValue>>* jsonMaterialParameters = nullptr;

		if (!jsonMaterialObject->TryGetStringField(GravityAssetInfoID::MaterialType, materialInfo.Type))
		{
			UE_LOG(LogGravityAssetTools, Warning, TEXT("The type of material '%s' is invalid."), *materialInfo.Name);

			return nullptr;
		}

		if (jsonMaterialObject->TryGetArrayField(GravityAssetInfoID::MaterialParameters, jsonMaterialParameters))
		{
			if (!ParseMaterialParameters(*jsonMaterialParameters, materialInfo.Name, materialInfo))
			{
				UE_LOG(LogGravityAssetTools, Warning, TEXT("The material '%s' has an invalid parameter section."), *materialInfo.Name);

				return nullptr;
			}
		}

		const TSharedPtr<FJsonObject>* jsonMaterialTexturesObject = nullptr;

		if (jsonMaterialObject->TryGetObjectField(GravityAssetInfoID::MaterialTextures, jsonMaterialTexturesObject))
		{
			for (const auto& materialTextureEntry : (*jsonMaterialTexturesObject)->Values)
			{
				FGravityTextureInfo materialTextureInfo;

				FString materialTextureChannelName = materialTextureEntry.Key;

				if (!materialTextureChannelName.RemoveFromStart(TEXT("Texture"), ESearchCase::CaseSensitive) || !materialTextureChannelName.IsNumeric())
				{
					WarnFailedToParseMaterialTextureFieldLambda(*materialInfo.Name, *materialTextureEntry.Key);

					return nullptr;
				}

				int32 textureChannelBinding = 0;

				bool bIsTextureChannelBindingValid = FDefaultValueHelper::ParseInt(materialTextureChannelName, textureChannelBinding);

				checkf(bIsTextureChannelBindingValid, TEXT("illformated number string!"));

				const TSharedPtr<FJsonObject>& jsonMaterialTextureObject = materialTextureEntry.Value->AsObject();

				TSharedPtr<FJsonValue> jsonMaterialTextureName = jsonMaterialTextureObject->TryGetField(GravityAssetInfoID::MaterialTextureName);

				if (!jsonMaterialTextureName.IsValid() || !(jsonMaterialTextureName->Type == EJson::String))
				{
					WarnFailedToParseMaterialTextureFieldLambda(*materialInfo.Name, *materialTextureEntry.Key);

					return nullptr;
				}

				materialTextureInfo.Name = jsonMaterialTextureName->AsString();

				if (materialTextureInfo.Name.Compare(TEXT("No Texture"), ESearchCase::IgnoreCase) == 0)
				{
					continue;
				}

				const TArray<TSharedPtr<FJsonValue>>* jsonMaterialTexureParameters = nullptr;

				if (!jsonMaterialTextureObject->TryGetArrayField(GravityAssetInfoID::MaterialTextureParameters, jsonMaterialTexureParameters))
				{
					WarnFailedToParseMaterialTextureFieldLambda(*materialInfo.Name, *materialTextureEntry.Key);

					return nullptr;
				}

				if (jsonMaterialTexureParameters->Num() != 3)
				{
					WarnFailedToParseMaterialTextureFieldLambda(*materialInfo.Name, *materialTextureEntry.Key);

					return nullptr;
				}

				materialTextureInfo.Parameters[0] = static_cast<float>((*jsonMaterialTexureParameters)[0]->AsNumber());
				materialTextureInfo.Parameters[1] = static_cast<float>((*jsonMaterialTexureParameters)[1]->AsNumber());
				materialTextureInfo.Parameters[2] = static_cast<float>((*jsonMaterialTexureParameters)[2]->AsNumber());

				const TArray<TSharedPtr<FJsonValue>>* jsonMaterialTexureAtlasParameters = nullptr;

				if (jsonMaterialTextureObject->TryGetArrayField(GravityAssetInfoID::MaterialTextureAtlasParameters, jsonMaterialTexureAtlasParameters))
				{
					if (jsonMaterialTexureAtlasParameters->Num() != 4)
					{
						WarnFailedToParseMaterialTextureFieldLambda(*materialInfo.Name, *materialTextureEntry.Key);

						return nullptr;
					}

					materialTextureInfo.UVScale[0] = static_cast<float>((*jsonMaterialTexureAtlasParameters)[0]->AsNumber());
					materialTextureInfo.UVScale[1] = static_cast<float>((*jsonMaterialTexureAtlasParameters)[1]->AsNumber());
					materialTextureInfo.UVOffset[0] = static_cast<float>((*jsonMaterialTexureAtlasParameters)[2]->AsNumber());
					materialTextureInfo.UVOffset[1] = static_cast<float>((*jsonMaterialTexureAtlasParameters)[3]->AsNumber());
				}

				materialInfo.Textures.Add(textureChannelBinding, MoveTemp(materialTextureInfo));
			}
		}

		gravityAssetInfo->MaterialInfos.Add(materialInfo.Name, MoveTemp(materialInfo));
	}

	return gravityAssetInfo;
}

/**
 * Enables nanite support for a static mesh.
 * @param StaticMesh The mesh which should support nanite.
 */
static void EnableNaniteSupportForStaticMesh(UStaticMesh* StaticMesh)
{
	StaticMesh->NaniteSettings.bEnabled = true;
	StaticMesh->PostEditChange();
}

/**
 * Imports a texture from file.
 * @param TextureFilePath The path to the source texture file.
 * @returns The imported texture on success.
 */
static UTexture* ImportTexture(const FString& TextureFilePath, const FString& OutputTextureDir)
{
	static IAssetTools& AssetTools = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	const FString textureName = FPaths::GetBaseFilename(TextureFilePath);
	const FString textureFileName = FString::Printf(TEXT("T_%s"), *textureName);

	FString textureFilePath = FString::Format(TEXT("Texture'{0}/{1}.{1}'"), { OutputTextureDir, textureFileName });

	TObjectPtr<UTexture> loadedTexture = LoadObject<UTexture>(nullptr, *textureFilePath, nullptr, LOAD_EditorOnly | LOAD_NoWarn, nullptr);

	if (loadedTexture)
	{
		return loadedTexture;
	}

	TObjectPtr<UTextureFactory> textureFactory = UTextureFactory::StaticClass()->GetDefaultObject<UTextureFactory>();

	TObjectPtr<UAssetImportTask> assetImportTask = NewObject<UAssetImportTask>();
	assetImportTask->bAutomated = true;
	assetImportTask->bReplaceExisting = true;
	assetImportTask->bSave = false;
	assetImportTask->Filename = TextureFilePath;
	assetImportTask->DestinationPath = OutputTextureDir;
	assetImportTask->Factory = textureFactory;
	assetImportTask->DestinationName = textureFileName;

	AssetTools.ImportAssetTasks({ assetImportTask });

	TArray<UObject*> importedObjects = assetImportTask->GetObjects();

	if (importedObjects.IsEmpty())
	{
		UE_LOG(LogGravityAssetTools, Warning, TEXT("Failed to load texture '%s'."), *TextureFilePath);

		return nullptr;
	}

	return Cast<UTexture>(importedObjects[0]);
}

/**
 * Creates a material instance from the material info.
 * @param AssetTools Asset tools used for import.
 * @param MaterilInfo The description of the new material.
 * @param BaseMaterialDir The directory containing all base materials.
 * @param OutputMaterialInstanceDir The directory wich will store the new material instance.
 * @returns The created or loaded material instance or nullptr.
 */
static UMaterialInstanceConstant* CreateMaterialInstance(
	IAssetTools& AssetTools, const FGravityMaterialInfo& MaterialInfo, const FString& BaseMaterialDir, const FString& OutputMaterialInstanceDir)
{
	FString materialInstanceName = TEXT("MI_") + MaterialInfo.Name.Replace(TEXT(" "), TEXT("_"), ESearchCase::CaseSensitive);
	FString materialInstanceFilePath = FString::Format(TEXT("MaterialInstanceConstant'{0}/{1}.{1}'"), { OutputMaterialInstanceDir, materialInstanceName });

	TObjectPtr<UMaterialInstanceConstant> materialInstance = LoadObject<UMaterialInstanceConstant>(nullptr, *materialInstanceFilePath, nullptr, LOAD_EditorOnly | LOAD_NoWarn, nullptr);

	if (materialInstance)
	{
		return materialInstance;
	}

	// determine the type of base material and load it
	FString baseMaterialFilePath = FString::Format(TEXT("Material'{0}/M_Type{1}.M_Type{1}'"), { BaseMaterialDir, MaterialInfo.Type });

	UMaterial* baseMaterial = LoadObject<UMaterial>(nullptr, *baseMaterialFilePath, nullptr, LOAD_EditorOnly, nullptr);

	if (!baseMaterial)
	{
		UE_LOG(LogGravityAssetTools, Warning, TEXT("Could not find base material '%s' for material instance '%s'."), *baseMaterialFilePath, *MaterialInfo.Name);

		return nullptr;
	}

	UMaterialInstanceConstantFactoryNew* materialInstanceFactory = UMaterialInstanceConstantFactoryNew::StaticClass()->GetDefaultObject<UMaterialInstanceConstantFactoryNew>();
	materialInstanceFactory->InitialParent = baseMaterial;

	materialInstance = Cast<UMaterialInstanceConstant>(AssetTools.CreateAsset(
		materialInstanceName, OutputMaterialInstanceDir, UMaterialInstanceConstant::StaticClass(), materialInstanceFactory));

	if (!materialInstance)
	{
		UE_LOG(LogGravityAssetTools, Warning, TEXT("Could not create a material instance for material '%s'."), *MaterialInfo.Name);
	}

	return materialInstance;
}

static void SetupMaterials(IAssetTools& AssetTools, UStaticMesh* StaticMesh, const FGravityAssetImportTask& AssetImportTask, const FGravityAssetInfo& GravityAssetInfo)
{
	TArray<FStaticMaterial>& staticMaterials = StaticMesh->GetStaticMaterials();

	FScopedSlowTask SlowTask(staticMaterials.Num(), LOCTEXT("SetupMaterialsSlowTask", "SetupMaterials"));
	SlowTask.MakeDialog();

	FString materialInstanceDir = FPaths::Combine(AssetImportTask.OutputMeshDir, GravityAssetInfo.Name, TEXT("Materials"));

	for (auto& staticMaterial : staticMaterials)
	{
		FString materialSlotName = staticMaterial.MaterialSlotName.ToString();
		const FGravityMaterialInfo* gravityMaterialInfo = GravityAssetInfo.MaterialInfos.Find(materialSlotName);

		if (!gravityMaterialInfo)
		{
			UE_LOG(LogGravityAssetTools, Warning, TEXT("Could not find material info for material slot '%s' of mesh '%s'."), *materialSlotName, *(StaticMesh->GetName()));

			continue;
		}

		SlowTask.EnterProgressFrame(1, FText::Format(LOCTEXT("SetupMaterials_CreateMaterial", "Creating \"{0}\"..."), FText::FromString(materialSlotName)));

		// @fixme - setup only material M_Type23 for now
		if (gravityMaterialInfo->Type != TEXT("23"))
		{
			continue;
		}

		TObjectPtr<UMaterialInstanceConstant> materialInstance =
			CreateMaterialInstance(AssetTools, *gravityMaterialInfo, AssetImportTask.BaseMaterialDir, materialInstanceDir);

		// sanity check if the material has the correct number of texture channels.
		TArray<FMaterialParameterInfo> textureParameterInfos;
		TArray<FGuid> textureParameterIDs;

		materialInstance->GetAllTextureParameterInfo(textureParameterInfos, textureParameterIDs);

		// @fixme - fix this check
		//if (textureParameterInfos.Num() != gravityMaterialInfo->Textures.Num())
		//{
		//	UE_LOG(LogGravityAssetTools, Warning,
		//		TEXT("The material '%s' of mesh '%s' has an invalid number of texture channels. Textures for missing channels will be ignored."),
		//		*(gravityMaterialInfo->Name), *(StaticMesh->GetName()));
		//}

		// @todo - assign debugging material for broken material setups

		staticMaterial.MaterialInterface = materialInstance;

		// import and assign textures to material channels

		// @fixme - remove this lambda
		auto GetTextureChannelByIndexLambda = [&textureParameterInfos](int32 Index, bool& bIsSecondaryTexture, bool& bIsEmissiveMaskTexture) -> const FMaterialParameterInfo*
		{
			const TCHAR* textureChannelName = nullptr;

			bIsEmissiveMaskTexture = false;
			bIsSecondaryTexture = false;

			switch (Index)
			{
			case 1: { textureChannelName = TEXT("Albedo");											} break;
			case 2: { textureChannelName = TEXT("Normal");											} break;
			case 3: { textureChannelName = TEXT("EmissionMask");	bIsEmissiveMaskTexture = true;  } break;
			case 4: { textureChannelName = TEXT("SecondaryAlbedo"); bIsSecondaryTexture = true;     } break;
			case 5: { textureChannelName = TEXT("SecondaryNormal"); bIsSecondaryTexture = true;     } break;
			}

			if (!textureChannelName)
			{
				return nullptr;
			}

			for (const auto& textureParameterInfo : textureParameterInfos)
			{
				if (textureParameterInfo.Name == textureChannelName)
				{
					return &textureParameterInfo;
				}
			}

			return nullptr;
		};

		bool bIsMaterialLayered = false;
		bool bIsMaterialEmissive = false;

		FString sourceTextureFilePath;

		for (const auto& entry : gravityMaterialInfo->Textures)
		{
			int32 textureChannelIndex = entry.Key;

			bool bIsSecondaryTexture = false;
			bool bIsEmissiveMaskTexture = false;

			const FMaterialParameterInfo* textureParameterInfo = GetTextureChannelByIndexLambda(textureChannelIndex, bIsSecondaryTexture, bIsEmissiveMaskTexture);

			bIsMaterialLayered |= bIsSecondaryTexture;
			bIsMaterialEmissive |= bIsEmissiveMaskTexture;

			if (!textureParameterInfo)
			{
				UE_LOG(LogGravityAssetTools, Warning, TEXT("Could not find appropriate texture channel in material '%s'."), *(gravityMaterialInfo->Name));

				continue;
			}

			const FGravityTextureInfo& textureInfo = entry.Value;

			sourceTextureFilePath = FPaths::Combine(AssetImportTask.SourceTextureDir, textureInfo.Name + TEXT(".png"));

			TObjectPtr<UTexture> importedTexture = ImportTexture(sourceTextureFilePath, AssetImportTask.OutputTextureDir);

			if (importedTexture)
			{
				materialInstance->SetTextureParameterValueEditorOnly(*textureParameterInfo, importedTexture);
			}
		}

		if (bIsMaterialLayered)
		{
			materialInstance->SetStaticSwitchParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Layered")), bIsMaterialLayered);
		}

		if (bIsMaterialEmissive)
		{
			materialInstance->SetStaticSwitchParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Emissive")), bIsMaterialEmissive);
		}
	}
}

bool UGravityAssetTools::IsImportDirectoryValid(const FString& ImportDir, FString& OutFailReason)
{
	OutFailReason.Reset();

	static IFileManager& fileManager = FFileManagerGeneric::Get();

	if (ImportDir.IsEmpty())
	{
		OutFailReason = TEXT("Path to the import directory is invalid.");

		return false;
	}

	if (!fileManager.DirectoryExists(*ImportDir))
	{
		OutFailReason = FString::Printf(TEXT("The import directory '%s' does not exist."), *ImportDir);

		return false;
	}

	FString assetName = FPaths::GetBaseFilename(ImportDir);

	FString assetSourceFilePath = FPaths::Combine(ImportDir, assetName + ".fbx");
	FString assetInfoFilePath = FPaths::Combine(ImportDir, assetName + ".json");

	if (!fileManager.FileExists(*assetSourceFilePath))
	{
		OutFailReason = FString::Printf(TEXT("The asset directory '%s' is missing a '%s.fbx' file."), *ImportDir, *assetName);
	
		return false;
	}

	if (!fileManager.FileExists(*assetInfoFilePath))
	{
		OutFailReason = FString::Printf(TEXT("The asset directory '%s' must contain a '%s.json' file."), *ImportDir, *assetName);

		return false;
	}

	return true;
}

int32 UGravityAssetTools::ImportAssetTasks(const TArray<FGravityAssetImportTask>& ImportTasks, int32 ImportBatchSize, bool bSavePackages)
{
	if (ImportTasks.IsEmpty())
	{
		return 0;
	}

	static IAssetTools& AssetTools = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	struct FAssetImportInfo
	{
		FAssetImportInfo(const FGravityAssetImportTask* InAssetImportTask, const FGravityAssetInfoPtr& InGravityAssetInfo)
			: AssetImportTask(InAssetImportTask)
			, GravityAssetInfo(InGravityAssetInfo)
		{}

		const FGravityAssetImportTask* AssetImportTask;
		const FGravityAssetInfoPtr GravityAssetInfo;
	};

	TArray<FAssetImportInfo> assetImportInfos;

	for (const auto& importTask : ImportTasks)
	{
		FGravityAssetInfoPtr gravityAssetInfo = LoadGravityAssetInfoFromDirectory(importTask.SourceMeshDir);

		if (!gravityAssetInfo)
		{
			// something went wrong, skip this asset
			continue;
		}

		FString fbxFilename = gravityAssetInfo->Name;
		fbxFilename.ToLowerInline();

		FString outputMeshContentDir = FPaths::Combine(importTask.OutputMeshDir, fbxFilename);

		// check if all static meshes are imported already
		bool bAssetRequiresReimport = false;
		
		for (const auto& meshFilename : gravityAssetInfo->ModelNames)
		{
			FString meshFilePath = FString::Format(TEXT("StaticMesh'{0}/SM_{1}.SM_{1}'"), { outputMeshContentDir, meshFilename });

			if (!LoadObject<UStaticMesh>(nullptr, *meshFilePath, nullptr, LOAD_EditorOnly | LOAD_NoWarn, nullptr))
			{
				bAssetRequiresReimport = true;
				break;
			}
		}

		if (bAssetRequiresReimport)
		{
			assetImportInfos.Emplace(FAssetImportInfo(&importTask, gravityAssetInfo));
		}
	}

	FScopedSlowTask SlowTask(assetImportInfos.Num(), LOCTEXT("ModifySlowTask", "Modify"));
	SlowTask.MakeDialog();

	TObjectPtr<UFbxImportUI> fbxImportUI = NewObject<UFbxImportUI>();

	fbxImportUI->MeshTypeToImport = EFBXImportType::FBXIT_StaticMesh;
	fbxImportUI->bImportMaterials = false;
	fbxImportUI->bImportTextures = false;
	fbxImportUI->bCreatePhysicsAsset = false;
	fbxImportUI->bAutomatedImportShouldDetectType = false;
	fbxImportUI->bOverrideFullName = false;

	fbxImportUI->StaticMeshImportData->VertexColorImportOption = EVertexColorImportOption::Replace;
	fbxImportUI->StaticMeshImportData->bAutoGenerateCollision = false;

	// remove these paramters if you experience issues with normals and tanges of the mesh
	fbxImportUI->StaticMeshImportData->bComputeWeightedNormals = false;
	fbxImportUI->StaticMeshImportData->NormalImportMethod = EFBXNormalImportMethod::FBXNIM_ImportNormalsAndTangents;

	fbxImportUI->AddToRoot();

	TObjectPtr<UFbxFactory> fbxFactory = UFbxFactory::StaticClass()->GetDefaultObject<UFbxFactory>();

	TArray<TObjectPtr<UAssetImportTask>> assetImportTaskBatch;
	assetImportTaskBatch.Reserve(ImportBatchSize);

	TArray<FAssetImportInfo> assetImportInfoBatch;
	assetImportInfoBatch.Reserve(ImportBatchSize);

	int32 numImportedAssets = 0;

	int32 idx = 0;
	while (idx < assetImportInfos.Num())
	{
		// create a batch of import tasks
		do
		{
			const FAssetImportInfo& assetImportInfo = assetImportInfos[idx++];

			TObjectPtr<UAssetImportTask> assetImportTask = NewObject<UAssetImportTask>();

			assetImportTask->bAutomated = true;
			assetImportTask->Options = fbxImportUI;
			assetImportTask->bReplaceExisting = true;
			assetImportTask->bSave = false;
			assetImportTask->Filename = assetImportInfo.GravityAssetInfo->SourceFilePath;
			assetImportTask->DestinationPath = FPaths::Combine(assetImportInfo.AssetImportTask->OutputMeshDir, assetImportInfo.GravityAssetInfo->Name);
			assetImportTask->Factory = fbxFactory;
			assetImportTask->DestinationName = TEXT("SM"); // the static mesh prefix will be appended to the mesh name

			assetImportTaskBatch.Emplace(assetImportTask);
			assetImportInfoBatch.Emplace(assetImportInfo);
		} while (idx < ImportBatchSize && idx < assetImportTaskBatch.Num());

		AssetTools.ImportAssetTasks(assetImportTaskBatch);

		// process the import results of the import batch
		for (int32 idy = 0; idy < assetImportTaskBatch.Num(); ++idy)
		{
			const TObjectPtr<UAssetImportTask>& assetImportTask = assetImportTaskBatch[idy];
			const FAssetImportInfo& assetImportInfo = assetImportInfoBatch[idy];

			SlowTask.EnterProgressFrame(1, FText::Format(LOCTEXT("Modify_ModifyingAsset", "Modifying \"{0}\"..."), FText::FromString(assetImportInfo.GravityAssetInfo->Name)));

			const TArray<UObject*>& importedObjects = assetImportTask->GetObjects();

			for (UObject* importedObject : importedObjects)
			{
				TObjectPtr<UStaticMesh> importedStaticMesh = Cast<UStaticMesh>(importedObject);

				SetupMaterials(AssetTools, importedStaticMesh, *assetImportInfo.AssetImportTask, *assetImportInfo.GravityAssetInfo);

				EnableNaniteSupportForStaticMesh(importedStaticMesh);

				++numImportedAssets;
			}
		}

		assetImportTaskBatch.Reset();
		assetImportInfoBatch.Reset();

		// cleanup after performing a number of imports
		fbxFactory->CleanUp();

		// force garbace collection so we able to load new objects.
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

		if (bSavePackages)
		{
			FEditorFileUtils::SaveDirtyPackages(false, false, true);
		}
	}

	fbxImportUI->RemoveFromRoot();

	return numImportedAssets;
}

#undef LOCTEXT_NAMESPACE