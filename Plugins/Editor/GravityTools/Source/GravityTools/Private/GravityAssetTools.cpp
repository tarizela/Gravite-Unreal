#include "GravityAssetTools.h"

#include "GravityMaterialPresetDatabase.h"
#include "GravityAssetToolsTypes.h"

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
#include <PhysicsEngine/BodySetup.h>
#include <UObject/Package.h>
#include <IImageWrapperModule.h>
#include <ObjectTools.h>
#include <Engine/Texture2D.h>
#include <Engine/TextureCube.h>

#include <atomic>

FGravityAssetImportContext UGravityAssetTools::ImportContext = {};

#define LOCTEXT_NAMESPACE "UGravityAssetTools"

namespace GravityAssetInfoID
{
	static const TCHAR* Version							= TEXT("Version");
	static const TCHAR* Meshes							= TEXT("Meshes");
	static const TCHAR* Materials						= TEXT("Materials");
	static const TCHAR* MaterialType					= TEXT("Type");
	static const TCHAR* MaterialParameters				= TEXT("Parameters");
	static const TCHAR* MaterialTextures				= TEXT("Textures");
	static const TCHAR* MaterialTextureName				= TEXT("Name");
	static const TCHAR* MaterialTextureParameters		= TEXT("Parameters");
	static const TCHAR* MaterialTextureAtlasParameters	= TEXT("Atlas");
};

static int32 TextureBindingIndexToMaterialTextureChannel(EGravityMaterialType MaterialType, int32 TextureBindingIndex)
{
	switch (MaterialType)
	{
	case EGravityMaterialType::Standard:
	{
		switch (TextureBindingIndex)
		{
		case 1: return 0; // Albedo 0
		case 2: return 1; // Normal 0
		case 3: return 2; // Emission
		case 4: return 3; // Albedo 1
		case 5: return 4; // Normal 1
		}
	}break;
	case EGravityMaterialType::Translucent:
	{
		switch (TextureBindingIndex)
		{
		case 2: return 0; // Emission
		}
	}break;
	case EGravityMaterialType::TreeBranches:
	{
		switch (TextureBindingIndex)
		{
		case 1: return 0; // Albedo 0
		}
	}break;
	case EGravityMaterialType::Grass:
	{
		switch (TextureBindingIndex)
		{
		case 1: return 0; // Albedo 0
		case 2: return 1; // Normal 0
		}
	}break;
	case EGravityMaterialType::GravityCrystal:
	{
		switch (TextureBindingIndex)
		{
		case 1: return 0; // Emission
		case 2: return 1; // Normal 0
		}
	}break;
	case EGravityMaterialType::TV:
	{
		// does not have any material texture parameters
		return -1;
	}break;
	case EGravityMaterialType::Soil:
	{
		switch (TextureBindingIndex)
		{
		case 1: return 0; // Albedo 0
		case 2: return 1; // Albedo 1
		case 3: return 2; // Albedo 2
		case 4: return 3; // Albedo 3
		case 5: return 4; // Normal 0
		case 6: return 5; // Normal 1
		case 7: return 6; // Normal 2
		case 8: return 7; // Normal 3
		}
	}break;
	case EGravityMaterialType::Window:
	{
		switch (TextureBindingIndex)
		{
		case 1: return 0; // Glass
		case 2: return 1; // Glass Normal
		case 3: return 2; // Interior
		case 4: return 3; // Curtains
		}
	}break;
	case EGravityMaterialType::Water:
	{
		switch (TextureBindingIndex)
		{
		case 1: return 0; // Water Normal
		case 2: return 1; // Absorption
		case 4: return 2; // Caustics
		}
	}break;
	case EGravityMaterialType::Glass:
	{
		switch (TextureBindingIndex)
		{
		case 1: return 0; // Glass Normal
		}
	}break;
	case EGravityMaterialType::Decal:
	{
		switch (TextureBindingIndex)
		{
		case 1: return 0; // Albedo 0
		case 2: return 1; // Normal 0
		}
	}break;
	}

	return -1;
}

static EGravityMaterialParameterType ParameterIndexToGravityMaterialParameterType(EGravityMaterialType MaterialType, int32 ParameterIndex)
{
	switch (MaterialType)
	{
	case EGravityMaterialType::Standard:
	{
		switch (ParameterIndex)
		{
		case 2:  return EGravityMaterialParameterType::PresetType;
		case 3:  return EGravityMaterialParameterType::Roughness0;
		case 4:  return EGravityMaterialParameterType::Metallic0;
		case 5:  return EGravityMaterialParameterType::Specular0;
		case 6:  return EGravityMaterialParameterType::EmissionBoost0;
		case 7:  return EGravityMaterialParameterType::ParallaxHeight0;
		case 8:  return EGravityMaterialParameterType::Roughness1;
		case 9:  return EGravityMaterialParameterType::Metallic1;
		case 10: return EGravityMaterialParameterType::Specular1;
		case 11: return EGravityMaterialParameterType::EmissionBoost1;
		case 12: return EGravityMaterialParameterType::ParallaxHeight1;
		case 13: return EGravityMaterialParameterType::ColorR0;
		case 14: return EGravityMaterialParameterType::ColorG0;
		case 15: return EGravityMaterialParameterType::ColorB0;
		case 16: return EGravityMaterialParameterType::ColorR1;
		case 17: return EGravityMaterialParameterType::ColorG1;
		case 18: return EGravityMaterialParameterType::ColorB1;
		case 19: return EGravityMaterialParameterType::AlphaTest;
		}
	}break;
	case EGravityMaterialType::Decal:
	{
		// @todo - nee to figure out where the emission and color are stored
		switch (ParameterIndex)
		{
		case 9:  return EGravityMaterialParameterType::Roughness0;
		case 10: return EGravityMaterialParameterType::Metallic0;
		case 11: return EGravityMaterialParameterType::Specular0;
		case 12: return EGravityMaterialParameterType::AlphaTest;
		}
	}break;
	case EGravityMaterialType::Window:
	case EGravityMaterialType::Glass:
	case EGravityMaterialType::TV:
	{
		// has no parameters
	}break;
	case EGravityMaterialType::Soil:
	{
		// has two unknown parameters
	}break;
	}

	return EGravityMaterialParameterType::Unknown;
}

static bool ParseMaterialParameters(const TArray<TSharedPtr<FJsonValue>>& ParameterValues, const FString& MaterialName, FGravityMaterialInfo& OutMaterialInfo)
{
	if (ParameterValues.IsValidIndex(1))
	{
		const auto& materialFlagsParameter = ParameterValues[1];

		EGravityMaterialFlags materialFlags = EGravityMaterialFlags::UseFaceCulling;

		if (materialFlagsParameter->Type == EJson::Number)
		{
			materialFlags = static_cast<EGravityMaterialFlags>(materialFlagsParameter->AsNumber());
		}
		else
		{
			UE_LOG(LogGravityAssetTools, Warning, TEXT("The material '%s' is missing material flags. Will use default flags"), *MaterialName);
		}

		OutMaterialInfo.Flags = materialFlags;
	}

	// idx 0: unknown
	// idx 1: material flags
	for (int32 i = 2; i < ParameterValues.Num(); ++i)
	{
		EGravityMaterialParameterType parameterType = ParameterIndexToGravityMaterialParameterType(OutMaterialInfo.Type, i);

		const auto& jsonMaterialParameter = ParameterValues[i];

		switch (jsonMaterialParameter->Type)
		{
		case EJson::Number:	OutMaterialInfo.SetParameter(parameterType, MakeShared<FGravityMaterialParameterFloat>(jsonMaterialParameter->AsNumber())); break;
		case EJson::String:	OutMaterialInfo.SetParameter(parameterType, MakeShared<FGravityMaterialParameterString>(jsonMaterialParameter->AsString())); break;
		{
		}break;
		default:
			UE_LOG(LogGravityAssetTools, Warning, TEXT("Encountered an unexpected material parameter data type. Material: '%s', Parameter idx: '%d'."), *MaterialName, i);
		}
	}

	return true;
}

static bool IsAssetVersionSupported(const FString& AssetVersion)
{
	const FString importerVersion = UGravityAssetTools::GetVersionString();

	TArray<FString> assetVersionNumbers;
	TArray<FString> importerVersionNumbers;

	AssetVersion.ParseIntoArray(assetVersionNumbers, TEXT("."));
	importerVersion.ParseIntoArray(importerVersionNumbers, TEXT("."));

	if (assetVersionNumbers.IsEmpty() || assetVersionNumbers.Num() != importerVersionNumbers.Num())
	{
		UE_LOG(LogGravityAssetTools, Warning, TEXT("The version number of the asset is invalid."));

		return false;
	}

	if (assetVersionNumbers[0] != importerVersionNumbers[0] || assetVersionNumbers[1] != importerVersionNumbers[1])
	{
		UE_LOG(LogGravityAssetTools, Warning, TEXT("Asset with version '%s' is not supported by the importer '%s'."), *AssetVersion, *importerVersion);

		return false;
	}

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

	FString assetVersion;

	if (!jsonAssetInfoRootObject->TryGetStringField(GravityAssetInfoID::Version, assetVersion))
	{
		WarnFailedToParseFieldLambda(GravityAssetInfoID::Version);

		return nullptr;
	}

	if (!IsAssetVersionSupported(assetVersion))
	{
		UE_LOG(LogGravityAssetTools, Warning, TEXT("Asset: '%s' has an incompatible version number."), *assetInfoFilePath);

		return nullptr;
	}

	TArray<FString> assetMeshNames;

	if (!jsonAssetInfoRootObject->TryGetStringArrayField(GravityAssetInfoID::Meshes, assetMeshNames))
	{
		WarnFailedToParseFieldLambda(GravityAssetInfoID::Meshes);

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
	gravityAssetInfo->MeshNames = assetMeshNames;
	gravityAssetInfo->SourceFilePath = assetSourceFilePath;

	UE_LOG(LogGravityAssetTools, Display, TEXT("Load material info of model '%s'."), *gravityAssetInfo->Name);

	for (const auto& materialObjectEntry : (*jsonAssetMaterialsObject)->Values)
	{
		FGravityMaterialInfo materialInfo;

		FString virtualMaterialPath = materialObjectEntry.Key;

		int32 nameStringStart = 0;
		if (virtualMaterialPath.FindLastChar(TEXT(':'), nameStringStart))
		{
			materialInfo.Name = virtualMaterialPath.RightChop(nameStringStart + 1);
		}
		else
		{
			materialInfo.Name = virtualMaterialPath;
		}

		if (materialInfo.Name.IsEmpty())
		{
			UE_LOG(LogGravityAssetTools, Warning, TEXT("The asset '%s' has a material with no name. The material will not be processed."), *gravityAssetInfo->Name);

			return nullptr;
		}

		const TSharedPtr<FJsonObject>& jsonMaterialObject = materialObjectEntry.Value->AsObject();

		const TArray<TSharedPtr<FJsonValue>>* jsonMaterialParameters = nullptr;

		if (!jsonMaterialObject->TryGetStringField(GravityAssetInfoID::MaterialType, materialInfo.TypeString))
		{
			UE_LOG(LogGravityAssetTools, Warning, TEXT("The type of material '%s' is invalid."), *materialInfo.Name);

			return nullptr;
		}

		materialInfo.Type = StringToGravityMaterialType(materialInfo.TypeString);

		if (materialInfo.Type == EGravityMaterialType::Unknown)
		{
			UE_LOG(LogGravityAssetTools, Warning, TEXT("The material '%s' (type %s) is not supported."), *materialInfo.Name, *materialInfo.TypeString);
		
			continue;
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
				FGravityMaterialTextureInfo materialTextureInfo;

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

				materialTextureInfo.UVScale = FVector2D::UnitVector;

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

				materialTextureInfo.TextureChannel = TextureBindingIndexToMaterialTextureChannel(materialInfo.Type, textureChannelBinding);
				materialTextureInfo.bIsNormalMap = IsGravityMaterialChannelNormalMap(materialInfo.Type, materialTextureInfo.TextureChannel);

				materialInfo.SetTextureInfo(materialTextureInfo);
			}
		}

		gravityAssetInfo->MaterialInfos.Add(materialInfo.Name, MoveTemp(materialInfo));
	}

	return gravityAssetInfo;
}

/**
 * Configures the imported static mesh.
 * @param StaticMesh The mesh which should be setup.
 */
static void PostImportSetupStaticMesh(UStaticMesh* StaticMesh)
{
	bool bMarkAsDirty = false;

	// change physics setup
	UBodySetup* bodySetup = StaticMesh->GetBodySetup();
	
	if (bodySetup)
	{
		// @todo - we are currently using the complex mesh for simple collision traces. This will impact the performance. We should use more simple meshes (LOD) instead.
		if (bodySetup->CollisionTraceFlag != ECollisionTraceFlag::CTF_UseComplexAsSimple)
		{
			bodySetup->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseComplexAsSimple;

			FProperty* bodySetupProperty = FindFProperty<FProperty>(UStaticMesh::StaticClass(), UStaticMesh::GetBodySetupName());
			FPropertyChangedEvent bodySetupChangedEvent(bodySetupProperty);
			StaticMesh->PostEditChangeProperty(bodySetupChangedEvent);

			bMarkAsDirty = true;
		}
	}
	else
	{
		UE_LOG(LogGravityAssetTools, Warning, TEXT("The imported static mesh '%s' does not have a physical body. Collision will be disabled."), *StaticMesh->GetName());
	}

	bool bUseNanite = DoesStaticMeshSupportNanite(StaticMesh);
	bool bApplyNaniteSettings = false;

	if (StaticMesh->NaniteSettings.bEnabled != bUseNanite)
	{
		StaticMesh->NaniteSettings.bEnabled = bUseNanite;

		bApplyNaniteSettings = true;
	}

	if (StaticMesh->NaniteSettings.bEnabled &&
		(StaticMesh->NaniteSettings.FallbackTarget != ENaniteFallbackTarget::RelativeError ||
		StaticMesh->NaniteSettings.FallbackRelativeError > 0.0f))
	{
		// must be set because we use the graphical mesh as a collider and do not want the LOD generator to create holes in the mesh.
		StaticMesh->NaniteSettings.FallbackTarget = ENaniteFallbackTarget::RelativeError;
		StaticMesh->NaniteSettings.FallbackRelativeError = 0.0f;

		bApplyNaniteSettings = true;
	}

	if (bApplyNaniteSettings)
	{
		// apply nanite settings
		FProperty* naniteSettingsProperty = FindFProperty<FProperty>(UStaticMesh::StaticClass(), GET_MEMBER_NAME_CHECKED(UStaticMesh, NaniteSettings));
		FPropertyChangedEvent naniteSettingsChangedEvent(naniteSettingsProperty);
		StaticMesh->PostEditChangeProperty(naniteSettingsChangedEvent);

		bMarkAsDirty = true;
	}

	if (bMarkAsDirty)
	{
		StaticMesh->Modify();
	}
}

/**
 * Imports a texture from file. Can be a DDS or a PNG file.
 * @param TextureFilePath The path to the source texture file.
 * @returns The imported texture on success.
 */
static UTexture* ImportTexture(IAssetTools& AssetTools, const FString& SourceTextureDir, const FString& TextureFileName, const FString& OutputTextureDir, bool bIsNormalMap = false, ETextureSourceColorSpace ColorSpace = ETextureSourceColorSpace::Auto, bool bWarn = true)
{
	const FString textureName = ObjectTools::SanitizeObjectName(TextureFileName);
	const FString textureFileName = FString::Printf(TEXT("T_%s"), *textureName);

	FString textureAssetPath = FString::Format(TEXT("Texture'{0}/{1}.{1}'"), { OutputTextureDir, textureFileName });

	TObjectPtr<UTexture> loadedTexture = LoadObject<UTexture>(nullptr, *textureAssetPath, nullptr, LOAD_EditorOnly | LOAD_NoWarn, nullptr);

	if (loadedTexture)  
	{
		return loadedTexture;
	}

	// the importer supports only DDS (cube map) and PNG files
	FString textureFilePath = FPaths::Combine(SourceTextureDir, TextureFileName + TEXT(".dds"));

	if (!FPaths::FileExists(textureFilePath))
	{
		textureFilePath = FPaths::Combine(SourceTextureDir, TextureFileName + TEXT(".png"));

		if (!FPaths::FileExists(textureFilePath))
		{
			if (bWarn)
			{
				UE_LOG(LogGravityAssetTools, Warning, TEXT("Failed import texture '%s'. File does not exist."), *textureFilePath);
			}

			return nullptr;
		}
	}

	TObjectPtr<UTextureFactory> textureFactory = NewObject<UTextureFactory>(UTextureFactory::StaticClass());
	textureFactory->bDeferCompression = true;
	textureFactory->LODGroup = bIsNormalMap ? TextureGroup::TEXTUREGROUP_WorldNormalMap : TextureGroup::TEXTUREGROUP_World;
	textureFactory->CompressionSettings = bIsNormalMap ? TextureCompressionSettings::TC_Normalmap : TextureCompressionSettings::TC_Default;
	textureFactory->bUsingExistingSettings = bIsNormalMap;
	textureFactory->ColorSpaceMode = ColorSpace;

	textureFactory->AddToRoot();

	TObjectPtr<UAssetImportTask> assetImportTask = NewObject<UAssetImportTask>();
	assetImportTask->bAutomated = true;
	assetImportTask->bReplaceExisting = true;
	assetImportTask->bSave = false;
	assetImportTask->Filename = textureFilePath;
	assetImportTask->DestinationPath = OutputTextureDir;
	assetImportTask->Factory = textureFactory;
	assetImportTask->DestinationName = textureFileName;

	AssetTools.ImportAssetTasks({ assetImportTask });

	TArray<UObject*> importedObjects = assetImportTask->GetObjects();

	textureFactory->CleanUp();
	textureFactory->RemoveFromRoot();

	if (importedObjects.IsEmpty())
	{
		if (bWarn)
		{
			UE_LOG(LogGravityAssetTools, Warning, TEXT("Failed to load texture '%s'."), *textureFilePath);
		}

		return nullptr;
	}

	return Cast<UTexture>(importedObjects[0]);
}

static UTexture* CreateParallaxMapFromNormalMap(IAssetTools& AssetTools, const FString& SourceTextureDir, const FString& SourceFileName, const FString& OutputTextureDir, bool bSaveSourceParallaxMap)
{
	FString sourceParallaxMapFileName = SourceFileName + TEXT("_p");
	FString parallaxMapFileName = ObjectTools::SanitizeObjectName(sourceParallaxMapFileName);
	FString parallaxMapAssetFileName = FString::Printf(TEXT("T_%s"), *parallaxMapFileName);

	// try loading an existing parallax texture asset
	FString parallaxMapReference = FString::Format(TEXT("Texture2D'{0}/{1}.{1}'"), { OutputTextureDir, parallaxMapAssetFileName });

	TObjectPtr<UTexture2D> loadedParallaxTexture = LoadObject<UTexture2D>(nullptr, *parallaxMapReference, nullptr, LOAD_EditorOnly | LOAD_NoWarn, nullptr);

	if (loadedParallaxTexture)
	{
		return loadedParallaxTexture;
	}

	// try importing from an existing parallax texture
	TObjectPtr<UTexture> importedHeightmap = ImportTexture(AssetTools, SourceTextureDir, sourceParallaxMapFileName, OutputTextureDir, false, ETextureSourceColorSpace::Linear, false);

	if (importedHeightmap)
	{
		return importedHeightmap;
	}

	FString sourceFilePath = FPaths::Combine(SourceTextureDir, sourceParallaxMapFileName + TEXT(".png"));
	FString parallaxMapFilePath = FPaths::Combine(OutputTextureDir, parallaxMapAssetFileName);

	TArray64<uint8> normalMapDataCompressed;
	FFileHelper::LoadFileToArray(normalMapDataCompressed, *sourceFilePath);

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));

	FImage decompressedImage;
	if (!ImageWrapperModule.DecompressImage(normalMapDataCompressed.GetData(), normalMapDataCompressed.Num(), decompressedImage))
	{
		UE_LOG(LogGravityAssetTools, Warning, TEXT("Cannot create a parallax map. Failed to load normal texture '%s'."), *sourceFilePath);

		return nullptr;
	}

	if (decompressedImage.Format != ERawImageFormat::BGRA8)
	{
		UE_LOG(LogGravityAssetTools, Warning, TEXT("Cannot create a parallax map. The normal texture '%s' has the wrong pixel format. It needs to be BGRA8."), *sourceFilePath);

		return nullptr;
	}

	int64 numTexels = decompressedImage.GetNumPixels();
	int64 numTexelsPerJob = 0;
	int32 numJobs = ImageParallelForComputeNumJobsForPixels(numTexelsPerJob, numTexels);

	FImage parallaxMapInfo;
	parallaxMapInfo.Format = ERawImageFormat::G8;
	parallaxMapInfo.GammaSpace = EGammaSpace::Linear;
	parallaxMapInfo.SizeX = decompressedImage.SizeX;
	parallaxMapInfo.SizeY = decompressedImage.SizeY;
	parallaxMapInfo.NumSlices = 1;
	parallaxMapInfo.RawData.AddZeroed(numTexels);

	std::atomic_bool bIsOpaque = true;
	const auto& textureColor = decompressedImage.RawData;
	auto& textureAlpha = parallaxMapInfo.RawData;

	ParallelFor(TEXT("GravityAssetTools.ExtractAlphaChannel"), numJobs, 1, [numTexelsPerJob, numTexels, &textureColor, &textureAlpha, &bIsOpaque](int64 JobIndex)
	{
		const int64 startIndex = JobIndex * numTexelsPerJob;
		const int64 endIndex = FMath::Min(startIndex + numTexelsPerJob, numTexels);
		for (int64 texelIndex = startIndex; texelIndex < endIndex; ++texelIndex)
		{
			uint8& alphaValue = textureAlpha[texelIndex];

			alphaValue = textureColor[texelIndex * 4 + 3];

			// we have to threshold the alpha channel because of block compression artefacts
			if (alphaValue >= (255u - 8u))
			{
				alphaValue = 255u;
			}
			else
			{
				bIsOpaque = false;
			}
		}
	}, EParallelForFlags::Unbalanced);

	if (bIsOpaque)
	{
		// no point of creating a parallax map when the height info is missing.
		return nullptr;
	}

	TArray64<uint8> parallaxMapDataCompressed;
	if (!ImageWrapperModule.CompressImage(parallaxMapDataCompressed, EImageFormat::PNG, parallaxMapInfo))
	{
		UE_LOG(LogGravityAssetTools, Warning, TEXT("Cannot create a parallax map. Failed to compress the extracted parallax data '%s' as png."), *parallaxMapFileName);

		return nullptr;
	}

	if (bSaveSourceParallaxMap)
	{
		if (!FFileHelper::SaveArrayToFile(parallaxMapDataCompressed, *sourceFilePath))
		{
			UE_LOG(LogGravityAssetTools, Warning, TEXT("Failed to save the extracted parallax map as '%s'."), *sourceFilePath);
		}
	}

	UPackage* package = CreatePackage(*parallaxMapFilePath);

	if (!package)
	{
		UE_LOG(LogGravityAssetTools, Warning, TEXT("Failed to crete a package for the parallax map of normal texture '%s'."), *sourceFilePath);

		return nullptr;
	}

	package->FullyLoad();

	TObjectPtr<UTextureFactory> textureFactory = NewObject<UTextureFactory>(UTextureFactory::StaticClass());
	textureFactory->bDeferCompression = true;
	textureFactory->ColorSpaceMode = ETextureSourceColorSpace::Linear; // the alpha channel is not gamma corrected and we assume that the height map was created in linear color space.

	textureFactory->AddToRoot();

	const uint8* textureAlphaData = parallaxMapDataCompressed.GetData();
	TObjectPtr<UTexture2D> parallaxMapTextureAsset = Cast<UTexture2D>(textureFactory->FactoryCreateBinary(
		UTexture2D::StaticClass(),
		package,
		*parallaxMapAssetFileName,
		EObjectFlags::RF_Public | EObjectFlags::RF_Standalone | EObjectFlags::RF_Transactional,
		nullptr,
		TEXT("PNG"),
		textureAlphaData,
		textureAlphaData + parallaxMapDataCompressed.Num(),
		GWarn));

	textureFactory->CleanUp();
	textureFactory->RemoveFromRoot();

	if (parallaxMapTextureAsset)
	{
		parallaxMapTextureAsset->MarkPackageDirty();
	}

	return parallaxMapTextureAsset;
}

static UMaterial* LoadBaseMaterial(const FString& BaseMaterialDir, const FGravityMaterialInfo& MaterialInfo)
{
	FString baseMaterialFilePath = FString::Format(TEXT("Material'{0}/M_Type{1}.M_Type{1}'"), { BaseMaterialDir, MaterialInfo.TypeString });

	UMaterial* baseMaterial = LoadObject<UMaterial>(nullptr, *baseMaterialFilePath, nullptr, LOAD_EditorOnly, nullptr);

	if (!baseMaterial)
	{
		UE_LOG(LogGravityAssetTools, Warning, TEXT("Could not find base material '%s' for material instance '%s'."), *baseMaterialFilePath, *MaterialInfo.Name);

		return nullptr;
	}

	return baseMaterial;
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
	IAssetTools& AssetTools, FGravityMaterialRegistry& GravityMaterialRegistry, const FGravityMaterialInfo& MaterialInfo, const FString& BaseMaterialDir, const FString& OutputMaterialInstanceDir)
{
	FGravityMaterialRegistryItem* materialRegistryItem = GravityMaterialRegistry.FindMaterial(MaterialInfo);
	
	FString materialInstanceName;

	if (materialRegistryItem)
	{
		materialInstanceName = TEXT("MI_") + materialRegistryItem->MaterialAssetName;
	}
	else
	{
		UE_LOG(LogGravityAssetTools, Warning, TEXT("Material '%s' was not registered."), *MaterialInfo.Name);

		return nullptr;
	}

	FString materialInstanceFilePath;

	if (materialRegistryItem->MaterialAssetReference.IsEmpty())
	{
		materialInstanceFilePath = FString::Format(TEXT("MaterialInstanceConstant'{0}/{1}.{1}'"), { OutputMaterialInstanceDir, materialInstanceName });

		materialRegistryItem->MaterialAssetReference = materialInstanceFilePath;
	}
	else
	{
		materialInstanceFilePath = materialRegistryItem->MaterialAssetReference;
	}

	TObjectPtr<UMaterialInstanceConstant> materialInstance = LoadObject<UMaterialInstanceConstant>(nullptr, *materialInstanceFilePath, nullptr, LOAD_EditorOnly | LOAD_NoWarn, nullptr);

	if (materialInstance)
	{
		return materialInstance;
	}

	// determine the type of base material and load it
	UMaterial* baseMaterial = LoadBaseMaterial(BaseMaterialDir, MaterialInfo);

	if (!baseMaterial)
	{
		return nullptr;
	}

	UMaterialInstanceConstantFactoryNew* materialInstanceFactory = NewObject<UMaterialInstanceConstantFactoryNew>(UMaterialInstanceConstantFactoryNew::StaticClass());
	materialInstanceFactory->InitialParent = baseMaterial;

	materialInstanceFactory->AddToRoot();

	materialInstance = Cast<UMaterialInstanceConstant>(AssetTools.CreateAsset(
		materialInstanceName, OutputMaterialInstanceDir, UMaterialInstanceConstant::StaticClass(), materialInstanceFactory));

	materialInstanceFactory->CleanUp();
	materialInstanceFactory->RemoveFromRoot();

	if (!materialInstance)
	{
		UE_LOG(LogGravityAssetTools, Warning, TEXT("Could not create a material instance for material '%s'."), *MaterialInfo.Name);
	}

	return materialInstance;
}

static void SetMaterialTexture(UMaterialInstanceConstant& Material, const FGravityMaterialTextureInfo& TextureInfo, UTexture* Texture)
{
	FLinearColor uvScaleOffset;

	uvScaleOffset.R = TextureInfo.UVScale[0];
	uvScaleOffset.G = TextureInfo.UVScale[1];

	uvScaleOffset.B = TextureInfo.UVOffset[0];
	uvScaleOffset.A = TextureInfo.UVOffset[1];

	FMaterialParameterInfo uvScaleOffsetParameter(*FString::Format(TEXT("TextureUVScaleOffset{0}"), {TextureInfo.TextureChannel}));
	Material.SetVectorParameterValueEditorOnly(uvScaleOffsetParameter, uvScaleOffset);

	FMaterialParameterInfo textureParameter(*FString::Format(TEXT("Texture{0}"), { TextureInfo.TextureChannel }));
	Material.SetTextureParameterValueEditorOnly(textureParameter, Texture);
}

static void SetStandardMaterialParameters(UMaterialInstanceConstant& Material, const FGravityMaterialInfo& MaterialInfo)
{
	auto HasValidParallaxMapLambda = [](const UMaterialInstanceConstant& Material, int8 Level)
	{
		UTexture* parallaxHeightmap = nullptr;

		FHashedMaterialParameterInfo materialParameterInfo = 
			Level == 0 ? FHashedMaterialParameterInfo(TEXT("Texture5")) : FHashedMaterialParameterInfo(TEXT("Texture6"));

		Material.GetTextureParameterValue(materialParameterInfo, parallaxHeightmap, true);

		return parallaxHeightmap != nullptr;
	};

	bool bIsMaterialLayered = false;
	bool bHasEmissionMask = false;

	for (const auto& entry : MaterialInfo.GetTextureInfos())
	{
		const FGravityMaterialTextureInfo& textureInfo = entry.Value;

		switch (textureInfo.TextureChannel)
		{
		case 3:
		case 4:
		{
			bIsMaterialLayered = true;
		}break;
		case 2:
		{
			bHasEmissionMask = true;
		}break;
		}
	}

	const FGravityMaterialPresetDatabase& materialPresetDatabase = FGravityMaterialPresetDatabase::GetInstance();
	
	EGravityMaterialParameterPresetType parameterPresetType = materialPresetDatabase.GetPresetType(
		MaterialInfo.SafeGetStringParameter(EGravityMaterialParameterType::PresetType, TEXT("Unknown")));

	EGravityMaterialWorldLayer materialWorldLayer = EGravityMaterialWorldLayer::Primary;
	const FString primaryWorldLayerName = materialPresetDatabase.GetWorldLayerName(materialWorldLayer);

	if (!MaterialInfo.Name.StartsWith(primaryWorldLayerName))
	{
		materialWorldLayer = EGravityMaterialWorldLayer::Secondary;
	}

	const FGravityMaterialParameterPresets* presetsL0 = nullptr;
	const FGravityMaterialParameterPresets* presetsL1 = nullptr;

	if (parameterPresetType == EGravityMaterialParameterPresetType::Unknown)
	{
		if (materialWorldLayer == EGravityMaterialWorldLayer::Unknown)
		{
			// try to get the presets from the main layer
			materialWorldLayer = EGravityMaterialWorldLayer::Primary;
		}

		// try to get the parameter from the color texture
		const FGravityMaterialTextureInfo* albedoL0 = MaterialInfo.GetTextureInfo(0);
		const FGravityMaterialTextureInfo* albedoL1 = MaterialInfo.GetTextureInfo(3);

		if (albedoL0)
		{
			presetsL0 = &materialPresetDatabase.GetPresetForTexture(materialWorldLayer, albedoL0->Name);
		}

		if (albedoL1)
		{
			presetsL1 = &materialPresetDatabase.GetPresetForTexture(materialWorldLayer, albedoL1->Name);
		}
	}
	else
	{
		// get preset from the material context
		presetsL0 = &materialPresetDatabase.GetPreset(materialWorldLayer, parameterPresetType);
		presetsL1 = presetsL0;
	}

	float metallicL0 = 0.0f;
	float roughnessL0 = 0.0f;
	float specularL0 = 0.0f;

	if (!presetsL0)
	{
		metallicL0 = MaterialInfo.SafeGetFloatParameter(EGravityMaterialParameterType::Metallic0, 0.3f);
		roughnessL0 = MaterialInfo.SafeGetFloatParameter(EGravityMaterialParameterType::Roughness0, 0.7f);
		specularL0 = MaterialInfo.SafeGetFloatParameter(EGravityMaterialParameterType::Specular0, 0.0f);
	}
	else
	{
		metallicL0 = presetsL0->Metallic;
		roughnessL0 = presetsL0->Roughness;
		specularL0 = presetsL0->Specular;
	}

	float metallicL1 = 0.0f;
	float roughnessL1 = 0.0f;
	float specularL1 = 0.0f;

	if (!presetsL1)
	{
		metallicL1 = MaterialInfo.SafeGetFloatParameter(EGravityMaterialParameterType::Metallic1, 0.3f);
		roughnessL1 = MaterialInfo.SafeGetFloatParameter(EGravityMaterialParameterType::Roughness1, 0.7f);
		specularL1 = MaterialInfo.SafeGetFloatParameter(EGravityMaterialParameterType::Specular1, 0.0f);
	}
	else
	{
		metallicL1 = presetsL1->Metallic;
		roughnessL1 = presetsL1->Roughness;
		specularL1 = presetsL1->Specular;
	}

	// configure the material instance
	FLinearColor colorL0;
	colorL0.R = MaterialInfo.SafeGetFloatParameter(EGravityMaterialParameterType::ColorR0, 1.0f);
	colorL0.G = MaterialInfo.SafeGetFloatParameter(EGravityMaterialParameterType::ColorG0, 1.0f);
	colorL0.B = MaterialInfo.SafeGetFloatParameter(EGravityMaterialParameterType::ColorB0, 1.0f);
	colorL0.A = 1.0f;

	float parallaxHeightL0 = 0.125f * MaterialInfo.SafeGetFloatParameter(EGravityMaterialParameterType::ParallaxHeight0, 0.0f);
	float emissionBoostL0 = MaterialInfo.SafeGetFloatParameter(EGravityMaterialParameterType::EmissionBoost0, 0.0f);

	FLinearColor colorL1;
	colorL1.R = MaterialInfo.SafeGetFloatParameter(EGravityMaterialParameterType::ColorR1, 1.0f);
	colorL1.G = MaterialInfo.SafeGetFloatParameter(EGravityMaterialParameterType::ColorG1, 1.0f);
	colorL1.B = MaterialInfo.SafeGetFloatParameter(EGravityMaterialParameterType::ColorB1, 1.0f);
	colorL1.A = 1.0f;

	float parallaxHeightL1 = 0.125f * MaterialInfo.SafeGetFloatParameter(EGravityMaterialParameterType::ParallaxHeight1, 0.0f);
	float emissionBoostL1 = MaterialInfo.SafeGetFloatParameter(EGravityMaterialParameterType::EmissionBoost1, 0.0f);

	Material.SetVectorParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Color0")), colorL0);
	Material.SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Metallic0")), metallicL0);
	Material.SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Roughness0")), roughnessL0);
	Material.SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Specular0")), specularL0);

	Material.SetVectorParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Color1")), colorL1);
	Material.SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Metallic1")), metallicL1);
	Material.SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Roughness1")), roughnessL1);
	Material.SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Specular1")), specularL1);

	bool bIsColorEmissiveL0 = emissionBoostL0 > 0.0f && !colorL0.IsAlmostBlack();
	bool bIsColorEmissiveL1 = emissionBoostL1 > 0.0f && !colorL1.IsAlmostBlack();

	Material.SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("EmissionBoost0")), emissionBoostL0);
	Material.SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("EmissionBoost1")), emissionBoostL1);

	bool bIsEmissive = bHasEmissionMask || bIsColorEmissiveL0 || bIsColorEmissiveL1;

	Material.SetStaticSwitchParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Emissive")), bIsEmissive);

	bool bIsPOMEnabledL0 = false;
	bool bIsPOMEnabledL1 = false;

	if (parallaxHeightL0 > 0.0f)
	{
		bIsPOMEnabledL0 = HasValidParallaxMapLambda(Material, 0);
	}

	if (parallaxHeightL1 > 0.0f)
	{
		bIsPOMEnabledL1 = HasValidParallaxMapLambda(Material, 1);
	}

	Material.SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("ParallaxHeight0")), bIsPOMEnabledL0 ? parallaxHeightL0 : 0.0f);
	Material.SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("ParallaxHeight1")), bIsPOMEnabledL1 ? parallaxHeightL1 : 0.0f);

	Material.SetStaticSwitchParameterValueEditorOnly(FMaterialParameterInfo(TEXT("POM0")), bIsPOMEnabledL0);
	Material.SetStaticSwitchParameterValueEditorOnly(FMaterialParameterInfo(TEXT("POM1")), bIsPOMEnabledL1);

	bIsMaterialLayered |= bIsColorEmissiveL1 || bIsPOMEnabledL1;

	Material.SetStaticSwitchParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Layered")), bIsMaterialLayered);

	float alphaTestThreshold = MaterialInfo.SafeGetFloatParameter(EGravityMaterialParameterType::AlphaTest, 1.0f);

	bool bIsAlphaTestEnabled = EnumHasAnyFlags(MaterialInfo.Flags, EGravityMaterialFlags::AlphaTested) && alphaTestThreshold > 0.0f;

	Material.BasePropertyOverrides.bOverride_BlendMode = bIsAlphaTestEnabled;
	Material.BasePropertyOverrides.BlendMode = EBlendMode::BLEND_Masked;

	Material.BasePropertyOverrides.bOverride_OpacityMaskClipValue = bIsAlphaTestEnabled;
	Material.BasePropertyOverrides.OpacityMaskClipValue = FMath::Min(alphaTestThreshold, 0.96f); // we have to subtract a small value from the threshold due to artefacts of the block compression

	bool bIsTwoSided = !EnumHasAnyFlags(MaterialInfo.Flags, EGravityMaterialFlags::UseFaceCulling);

	Material.BasePropertyOverrides.bOverride_TwoSided = bIsTwoSided;
	Material.BasePropertyOverrides.TwoSided = true;
}

/*
static void SetSoilMaterialParameters(UMaterialInstanceConstant& Material, const FGravityMaterialInfo& MaterialInfo)
{
	UTexture* albedoL1Texture = nullptr;
	UTexture* albedoL2Texture = nullptr;
	UTexture* albedoL3Texture = nullptr;
	UTexture* normalL1Texture = nullptr;
	UTexture* normalL2Texture = nullptr;
	UTexture* normalL3Texture = nullptr;

	Material.GetTextureParameterValue(FHashedMaterialParameterInfo(*GravityMaterialChannelTypeToString(EGravityMaterialChannelType::AlbedoL1)), albedoL1Texture, true);
	Material.GetTextureParameterValue(FHashedMaterialParameterInfo(*GravityMaterialChannelTypeToString(EGravityMaterialChannelType::AlbedoL2)), albedoL2Texture, true);
	Material.GetTextureParameterValue(FHashedMaterialParameterInfo(*GravityMaterialChannelTypeToString(EGravityMaterialChannelType::AlbedoL3)), albedoL3Texture, true);
	Material.GetTextureParameterValue(FHashedMaterialParameterInfo(*GravityMaterialChannelTypeToString(EGravityMaterialChannelType::NormalL1)), normalL1Texture, true);
	Material.GetTextureParameterValue(FHashedMaterialParameterInfo(*GravityMaterialChannelTypeToString(EGravityMaterialChannelType::NormalL2)), normalL2Texture, true);
	Material.GetTextureParameterValue(FHashedMaterialParameterInfo(*GravityMaterialChannelTypeToString(EGravityMaterialChannelType::NormalL3)), normalL3Texture, true);

	Material.SetStaticSwitchParameterValueEditorOnly(FMaterialParameterInfo("UseLayer1"), albedoL1Texture || normalL1Texture);
	Material.SetStaticSwitchParameterValueEditorOnly(FMaterialParameterInfo("UseLayer2"), albedoL2Texture || normalL2Texture);
	Material.SetStaticSwitchParameterValueEditorOnly(FMaterialParameterInfo("UseLayer3"), albedoL3Texture || normalL3Texture);
}

static void SetWindowMaterialParameters(UMaterialInstanceConstant& Material, const FGravityMaterialInfo& MaterialInfo)
{
	UTexture* texture = nullptr;

	Material.GetTextureParameterValue(FHashedMaterialParameterInfo(*GravityMaterialChannelTypeToString(EGravityMaterialChannelType::InteriorMap)), texture, true);

	bool bIsLit = false;

	UTextureCube* interiorTexture = Cast<UTextureCube>(texture);

	check(!TEXT("fixme"));
	//if (interiorTexture)
	//{
	//	if (interiorTexture->GetPlatformData()->bSourceMipsAlphaDetectedValid)
	//	{
	//		bIsLit = interiorTexture->GetPlatformData()->bSourceMipsAlphaDetected;
	//	}
	//}

	Material.SetStaticSwitchParameterValueEditorOnly(FMaterialParameterInfo("IsLit"), bIsLit);
}
*/

static void SetMaterialParameters(UMaterialInstanceConstant& Material, const FGravityMaterialInfo& MaterialInfo)
{
	switch (MaterialInfo.Type)
	{
	case EGravityMaterialType::Standard:	{ SetStandardMaterialParameters(Material, MaterialInfo); } break;
	//case EGravityMaterialType::Window:		{ SetWindowMaterialParameters(Material, MaterialInfo);	 } break;
	//case EGravityMaterialType::Soil:		{ SetSoilMaterialParameters(Material, MaterialInfo);	 } break;
	}

	Material.UpdateStaticPermutation();
	Material.MarkPackageDirty();
}

static bool IsMaterialTypeSupported(EGravityMaterialType MaterialType)
{
	return MaterialType != EGravityMaterialType::Unknown;
}

static void SetupMaterials(IAssetTools& AssetTools, UStaticMesh* StaticMesh, const FGravityAssetImportTask& AssetImportTask, const FGravityAssetInfo& GravityAssetInfo, FGravityAssetImportContext& ImportContext)
{
	TArray<FStaticMaterial>& staticMaterials = StaticMesh->GetStaticMaterials();

	FScopedSlowTask SlowTask(staticMaterials.Num(), LOCTEXT("SetupMaterialsSlowTask", "SetupMaterials"));
	SlowTask.MakeDialog();

	FString debugMaterialFilePath = FString::Format(TEXT("Material'{0}/M_Invalid.M_Invalid'"), { AssetImportTask.BaseMaterialDir });
	TObjectPtr<UMaterial> debugMaterial = LoadObject<UMaterial>(nullptr, *debugMaterialFilePath, nullptr, LOAD_EditorOnly, nullptr);

	for (auto& staticMaterial : staticMaterials)
	{
		staticMaterial.MaterialInterface = debugMaterial;

		FString materialSlotName = staticMaterial.MaterialSlotName.ToString();
		const FGravityMaterialInfo* gravityMaterialInfo = GravityAssetInfo.MaterialInfos.Find(materialSlotName);

		if (!gravityMaterialInfo)
		{
			UE_LOG(LogGravityAssetTools, Warning, TEXT("Could not find material info for material slot '%s' of mesh '%s'."), *materialSlotName, *(StaticMesh->GetName()));

			continue;
		}

		SlowTask.EnterProgressFrame(1, FText::Format(LOCTEXT("SetupMaterials_CreateMaterial", "Creating \"{0}\"..."), FText::FromString(materialSlotName)));

		if (!IsMaterialTypeSupported(gravityMaterialInfo->Type))
		{
			UE_LOG(LogGravityAssetTools, Warning, TEXT("Material '%s' (Type %s) of mesh '%s' is currently not supported."), *materialSlotName, *gravityMaterialInfo->TypeString, *(StaticMesh->GetName()));

			continue;
		}

		TObjectPtr<UMaterialInstanceConstant> materialInstance = CreateMaterialInstance(AssetTools, ImportContext.GravityMaterialRegistry, *gravityMaterialInfo, AssetImportTask.BaseMaterialDir, AssetImportTask.OutputMaterialDir);

		if (!materialInstance)
		{
			// material instance creation failed
			continue;
		}

		ImportContext.PurgedPackages.Add(materialInstance->GetPackage());

		staticMaterial.MaterialInterface = materialInstance;

		// import textures and create parallax maps for the material
		TArray<TPair<TObjectPtr<UTexture>, FGravityMaterialTextureInfo>> materialTextures;

		for (const auto& entry : gravityMaterialInfo->GetTextureInfos())
		{
			const FGravityMaterialTextureInfo& textureInfo = entry.Value;

			TObjectPtr<UTexture> importedTexture = ImportTexture(AssetTools, AssetImportTask.SourceTextureDir, textureInfo.Name, AssetImportTask.OutputTextureDir, textureInfo.bIsNormalMap);

			if (importedTexture)
			{
				materialTextures.Emplace(importedTexture, textureInfo);
				ImportContext.PurgedPackages.Add(importedTexture->GetPackage());

				TObjectPtr<UTexture2D> importedTexture2D = Cast<UTexture2D>(importedTexture);

				if (importedTexture2D)
				{
					// check if we need to enable POM for this material.
					// we only enable POM if the texture of the normal map channels has with a valid alpha channel and the POM parametes of the material are not null.
					if (gravityMaterialInfo->Type == EGravityMaterialType::Standard)
					{
						// the Heigh map is stored in the alpha channel of the normal map and the bias value is inside the material definition.
						EGravityMaterialParameterType parallaxParameterChannel = EGravityMaterialParameterType::Unknown;
						int32 parallaxTextureChannel = -1;

						switch (textureInfo.TextureChannel)
						{
						case 1: // Normal 0
						{
							parallaxParameterChannel = EGravityMaterialParameterType::ParallaxHeight0;
							parallaxTextureChannel = 5;
						}break;
						case 4: // Normal 1
						{
							parallaxParameterChannel = EGravityMaterialParameterType::ParallaxHeight1;
							parallaxTextureChannel = 6;
						}break;
						}

						if (parallaxParameterChannel != EGravityMaterialParameterType::Unknown)
						{
							bool bIsParallaxEnabled = gravityMaterialInfo->SafeGetFloatParameter(parallaxParameterChannel, 0.0f) > 0.0f;
							if (bIsParallaxEnabled)
							{
								// create the parallax texture from the source file. We cannot use the normal texture asset because it is encoded as BC5 without alpha.
								TObjectPtr<UTexture> importedHeightmap =
									CreateParallaxMapFromNormalMap(AssetTools, AssetImportTask.SourceTextureDir, textureInfo.Name, AssetImportTask.OutputTextureDir, ImportContext.bSaveExtractedParallaxMaps);

								if (importedHeightmap)
								{
									ImportContext.PurgedPackages.Add(importedHeightmap->GetPackage());

									FGravityMaterialTextureInfo parallaxTextureInfo = textureInfo;
									parallaxTextureInfo.TextureChannel = parallaxTextureChannel;

									materialTextures.Emplace(importedHeightmap, MoveTemp(parallaxTextureInfo));
								}
							}
						}
					}
				}
			}
		}

		for (const auto& entry : materialTextures)
		{
			SetMaterialTexture(*materialInstance, entry.Value, entry.Key);
		}

		SetMaterialParameters(*materialInstance, *gravityMaterialInfo);
	}
}

static void MakeObjectPurgeable(UObject* InObject)
{
	if (InObject->IsRooted())
	{
		InObject->RemoveFromRoot();
	}

	InObject->ClearFlags(RF_Standalone);
}

static void MakePackagePurgeable(UPackage* InPackage)
{
	MakeObjectPurgeable(InPackage);

	ForEachObjectWithPackage(InPackage, [](UObject* InObject)
	{
		MakeObjectPurgeable(InObject);

		return true;
	});
}

FString UGravityAssetTools::GetVersionString()
{
	return TEXT("1.0.0");
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

int32 UGravityAssetTools::ImportAssetTasks(const TArray<FGravityAssetImportTask>& ImportTasks, int32 ImportBatchSize, bool bSavePackages, bool bSaveExtractedParallaxMaps, bool bPatchAssets)
{
	ImportContext.bSavePackages = bSavePackages;
	ImportContext.bSaveExtractedParallaxMaps = bSaveExtractedParallaxMaps;
	ImportContext.bPatchAssets = bPatchAssets;

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

	ImportContext.GravityMaterialRegistry.Reset();

	for (const auto& importTask : ImportTasks)
	{
		FGravityAssetInfoPtr gravityAssetInfo = LoadGravityAssetInfoFromDirectory(importTask.SourceMeshDir);

		if (!gravityAssetInfo)
		{
			// something went wrong, skip this asset
			continue;
		}

		ImportContext.GravityMaterialRegistry.RegisterAssetMaterials(*gravityAssetInfo);

		FString fbxFilename = gravityAssetInfo->Name;
		fbxFilename.ToLowerInline();

		FString outputMeshContentDir = FPaths::Combine(importTask.OutputMeshDir, fbxFilename);

		// check if all static meshes are imported already
		bool bAssetRequiresReimport = false;
		
		if (!ImportContext.bPatchAssets)
		{
			for (const auto& meshFilename : gravityAssetInfo->MeshNames)
			{
				FString meshFilePath = FString::Format(TEXT("StaticMesh'{0}/SM_{1}.SM_{1}'"), { outputMeshContentDir, ObjectTools::SanitizeObjectName(meshFilename) });

				if (!LoadObject<UStaticMesh>(nullptr, *meshFilePath, nullptr, LOAD_EditorOnly | LOAD_NoWarn, nullptr))
				{
					bAssetRequiresReimport = true;
					break;
				}
			}
		}
		else
		{
			bAssetRequiresReimport = !gravityAssetInfo->MeshNames.IsEmpty();
		}

		if (bAssetRequiresReimport)
		{
			assetImportInfos.Emplace(FAssetImportInfo(&importTask, gravityAssetInfo));
		}
	}

	FScopedSlowTask SlowTask(assetImportInfos.Num(), LOCTEXT("ImportSlowTask", "Import"));
	SlowTask.MakeDialog();

	TObjectPtr<UFbxImportUI> fbxImportUI = NewObject<UFbxImportUI>();

	fbxImportUI->MeshTypeToImport = EFBXImportType::FBXIT_StaticMesh;
	fbxImportUI->bImportMaterials = false;
	fbxImportUI->bImportTextures = false;
	fbxImportUI->bAutomatedImportShouldDetectType = false;
	fbxImportUI->bOverrideFullName = false;

	fbxImportUI->StaticMeshImportData->VertexColorImportOption = EVertexColorImportOption::Replace;
	fbxImportUI->StaticMeshImportData->bAutoGenerateCollision = false;

	// remove these paramters if you experience issues with normals and tanges of the mesh
	fbxImportUI->StaticMeshImportData->bComputeWeightedNormals = false;
	fbxImportUI->StaticMeshImportData->NormalImportMethod = EFBXNormalImportMethod::FBXNIM_ImportNormalsAndTangents;

	fbxImportUI->AddToRoot();

	TObjectPtr<UFbxFactory> fbxFactory = NewObject<UFbxFactory>(UFbxFactory::StaticClass());

	fbxFactory->AddToRoot();

	TArray<TObjectPtr<UAssetImportTask>> assetImportTaskBatch;
	assetImportTaskBatch.Reserve(ImportBatchSize);

	TArray<FAssetImportInfo> assetImportInfoBatch;
	assetImportInfoBatch.Reserve(ImportBatchSize);

	ImportContext.PurgedPackages.Reset();

	int32 numImportedAssets = 0;

	int32 idx = 0;
	while (idx < assetImportInfos.Num())
	{
		if (!ImportContext.bPatchAssets)
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

				SlowTask.EnterProgressFrame(1, FText::Format(LOCTEXT("Import_ModifyingAsset", "Modifying \"{0}\"..."), FText::FromString(assetImportInfo.GravityAssetInfo->Name)));

				UE_LOG(LogGravityAssetTools, Display, TEXT("Modifying '%s'."), *assetImportInfo.GravityAssetInfo->Name);

				const TArray<UObject*>& importedObjects = assetImportTask->GetObjects();

				for (UObject* importedObject : importedObjects)
				{
					TObjectPtr<UStaticMesh> importedStaticMesh = Cast<UStaticMesh>(importedObject);

					SetupMaterials(AssetTools, importedStaticMesh, *assetImportInfo.AssetImportTask, *assetImportInfo.GravityAssetInfo, ImportContext);

					PostImportSetupStaticMesh(importedStaticMesh);

					ImportContext.PurgedPackages.Add(importedStaticMesh->GetPackage());

					++numImportedAssets;
				}
			}

			assetImportTaskBatch.Reset();
			assetImportInfoBatch.Reset();

			// cleanup after performing a number of imports
			fbxFactory->CleanUp();
		}
		else
		{
			// process the import results of the import batch
			for (int32 idy = 0; idy < ImportBatchSize && idx < assetImportInfos.Num(); ++idy)
			{
				const FAssetImportInfo& assetImportInfo = assetImportInfos[idx++];

				const FString& fbxFileName = assetImportInfo.GravityAssetInfo->Name;
				const FString& outputMeshDir = assetImportInfo.AssetImportTask->OutputMeshDir;

				SlowTask.EnterProgressFrame(1, FText::Format(LOCTEXT("Patch_PatchingAsset", "Patching \"{0}\"..."), FText::FromString(fbxFileName)));

				FString meshContentDir = FPaths::Combine(outputMeshDir, fbxFileName);

				UE_LOG(LogGravityAssetTools, Display, TEXT("Patching '%s'."), *meshContentDir);

				for (const auto& meshFilename : assetImportInfo.GravityAssetInfo->MeshNames)
				{
					FString meshFilePath = FString::Format(TEXT("StaticMesh'{0}/SM_{1}.SM_{1}'"), { meshContentDir, ObjectTools::SanitizeObjectName(meshFilename) });
				
					TObjectPtr<UStaticMesh> staticMesh = LoadObject<UStaticMesh>(nullptr, *meshFilePath, nullptr, LOAD_EditorOnly | LOAD_NoWarn, nullptr);

					if (staticMesh)
					{
						SetupMaterials(AssetTools, staticMesh, *assetImportInfo.AssetImportTask, *assetImportInfo.GravityAssetInfo, ImportContext);

						PostImportSetupStaticMesh(staticMesh);

						ImportContext.PurgedPackages.Add(staticMesh->GetPackage());

						++numImportedAssets;
					}
					else
					{
						UE_LOG(LogGravityAssetTools, Warning, TEXT("Failed to patch '%s'. Asset is missing, perform a full reimport."), *meshFilePath);
					}
				}
			}
		}

		if (bSavePackages)
		{
			FEditorFileUtils::SaveDirtyPackages(false, false, true);

			// make packages purgable by GC
			for (auto* package : ImportContext.PurgedPackages)
			{
				MakePackagePurgeable(package);
			}
		}

		ImportContext.PurgedPackages.Reset();

		// force garbace collection so we able to load new objects.
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	}

	fbxImportUI->RemoveFromRoot();
	fbxFactory->RemoveFromRoot();

	return numImportedAssets;
}

#undef LOCTEXT_NAMESPACE