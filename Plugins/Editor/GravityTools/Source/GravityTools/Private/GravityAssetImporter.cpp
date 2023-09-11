#include "GravityAssetImporter.h"

#include "GravityAssetImporterArguments.h"

#include "GravityAssetImporterListView.h"

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
#include <Engine/StaticMesh.h>
#include <AssetImportTask.h>

DEFINE_LOG_CATEGORY(LogGravityAssetImporter)

#define LOCTEXT_NAMESPACE "SGravityAssetImporter"

namespace GravityAssetImporterAssetInfoID
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

static bool ParseMaterialParameters(const TArray<TSharedPtr<FJsonValue>>& ParameterValues, const FString& MaterialName, FGravityAssetImporterMaterialInfo& OutMaterialInfo)
{
	TArray<FGravityAssetImporterMaterialParameterPtr> materialParameters;

	for (int32 i = 0; i < ParameterValues.Num(); ++i)
	{
		const auto& jsonMaterialParameter = ParameterValues[i];

		switch (jsonMaterialParameter->Type)
		{
		case EJson::Number:	materialParameters.Emplace(MakeShared<FGravityAssetImporterMaterialParameterFloat>(jsonMaterialParameter->AsNumber())); break;
		case EJson::String:	materialParameters.Emplace(MakeShared<FGravityAssetImporterMaterialParameterString>(jsonMaterialParameter->AsString())); break;
		{
		}break;
		default:
			UE_LOG(LogGravityAssetImporter, Warning, TEXT(""), *MaterialName, i);
			return false;
		}
	}

	OutMaterialInfo.Parameters = MoveTemp(materialParameters);

	return true;
}

static FGravityAssetImporterAssetInfoPtr LoadGravityAssetInfoFromDirectory(const FString& AssetDirectory)
{
	// the asset directory must contain a json file with the same name as the directory with the info of the asset.
	FString assetName = FPaths::GetBaseFilename(AssetDirectory);

	FString assetInfoFilePath = FPaths::Combine(AssetDirectory, assetName + ".json");

	FString jsonAssetInfoString;

	if (!FFileHelper::LoadFileToString(jsonAssetInfoString, *assetInfoFilePath))
	{
		UE_LOG(LogGravityAssetImporter, Warning, TEXT("Skipping asset directory: '%s'. The directory must contain a '%s'.json file."), *AssetDirectory, *assetName);

		return nullptr;
	}

	TSharedRef<TJsonReader<TCHAR>> jsonAssetInfoFileReader = TJsonReaderFactory<TCHAR>::Create(jsonAssetInfoString);

	TSharedPtr<FJsonObject> jsonAssetInfoRootObject;

	if (!FJsonSerializer::Deserialize(jsonAssetInfoFileReader, jsonAssetInfoRootObject))
	{
		UE_LOG(LogGravityAssetImporter, Warning, TEXT("Failed to parse: '%s'."), *assetInfoFilePath);

		return nullptr;
	}

	TArray<FString> assetModelNames;

	const auto WarnFailedToParseFieldLambda = [&assetInfoFilePath](const TCHAR* ParsedFiledName)
	{
		UE_LOG(LogGravityAssetImporter, Warning, TEXT("Failed to parse: '%s'. The info file must contain the filed '%s'."), *assetInfoFilePath, ParsedFiledName);
	};

	const auto WarnFailedToParseMaterialFieldLambda = [](const TCHAR* MaterialName, const TCHAR* ParsedFiledName)
	{
		UE_LOG(LogGravityAssetImporter, Warning, TEXT("Failed to parse material '%s'. The material must contain the filed '%s'."), MaterialName, ParsedFiledName);
	};

	const auto WarnFailedToParseMaterialTextureFieldLambda = [](const TCHAR* MaterialName, const TCHAR* TextureName)
	{
		UE_LOG(LogGravityAssetImporter, Warning, TEXT("The material '%s' has an illformated texture parameter '%s'."), MaterialName, TextureName);
	};

	if (!jsonAssetInfoRootObject->TryGetStringArrayField(GravityAssetImporterAssetInfoID::ModelNames, assetModelNames))
	{
		WarnFailedToParseFieldLambda(GravityAssetImporterAssetInfoID::ModelNames);

		return nullptr;
	}

	FString assetColliderName;

	if (!jsonAssetInfoRootObject->TryGetStringField(GravityAssetImporterAssetInfoID::ColliderName, assetColliderName))
	{
		WarnFailedToParseFieldLambda(GravityAssetImporterAssetInfoID::ColliderName);

		return nullptr;
	}

	const TSharedPtr<FJsonObject>* jsonAssetMaterialsObject = nullptr;

	if (!jsonAssetInfoRootObject->TryGetObjectField(GravityAssetImporterAssetInfoID::Materials, jsonAssetMaterialsObject))
	{
		WarnFailedToParseFieldLambda(GravityAssetImporterAssetInfoID::Materials);

		return nullptr;
	}

	FGravityAssetImporterAssetInfoPtr gravityAssetInfo = MakeShared<FGravityAssetImporterAssetInfo>();

	gravityAssetInfo->Name = assetName;
	gravityAssetInfo->ModelNames = assetModelNames;
	gravityAssetInfo->ColliderName = assetColliderName;

	for (const auto& materialObjectEntry : (*jsonAssetMaterialsObject)->Values)
	{
		FGravityAssetImporterMaterialInfo materialInfo;

		materialInfo.Name = materialObjectEntry.Key;

		const TSharedPtr<FJsonObject>& jsonMaterialObject = materialObjectEntry.Value->AsObject();

		const TArray<TSharedPtr<FJsonValue>>* jsonMaterialParameters = nullptr;

		if (!jsonMaterialObject->TryGetStringField(GravityAssetImporterAssetInfoID::MaterialType, materialInfo.Type))
		{
			UE_LOG(LogGravityAssetImporter, Warning, TEXT("The type of material '%s' is invalid."), *materialInfo.Name);

			return nullptr;
		}

		if (jsonMaterialObject->TryGetArrayField(GravityAssetImporterAssetInfoID::MaterialParameters, jsonMaterialParameters))
		{
			if (!ParseMaterialParameters(*jsonMaterialParameters, materialInfo.Name, materialInfo))
			{
				UE_LOG(LogGravityAssetImporter, Warning, TEXT("The material '%s' has an invalid parameter section."), *materialInfo.Name);

				return nullptr;
			}
		}

		const TSharedPtr<FJsonObject>* jsonMaterialTexturesObject = nullptr;

		if (jsonMaterialObject->TryGetObjectField(GravityAssetImporterAssetInfoID::MaterialTextures, jsonMaterialTexturesObject))
		{
			for (const auto& materialTextureEntry : (*jsonMaterialTexturesObject)->Values)
			{
				FGravityAssetImporterTextureInfo materialTextureInfo;

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

				TSharedPtr<FJsonValue> jsonMaterialTextureName = jsonMaterialTextureObject->TryGetField(GravityAssetImporterAssetInfoID::MaterialTextureName);

				if (!jsonMaterialTextureName.IsValid() || !(jsonMaterialTextureName->Type == EJson::String))
				{
					WarnFailedToParseMaterialTextureFieldLambda(*materialInfo.Name, *materialTextureEntry.Key);

					return nullptr;
				}

				materialTextureInfo.Name = jsonMaterialTextureName->AsString();

				const TArray<TSharedPtr<FJsonValue>>* jsonMaterialTexureParameters = nullptr;

				if (!jsonMaterialTextureObject->TryGetArrayField(GravityAssetImporterAssetInfoID::MaterialTextureParameters, jsonMaterialTexureParameters))
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

				if (jsonMaterialTextureObject->TryGetArrayField(GravityAssetImporterAssetInfoID::MaterialTextureAtlasParameters, jsonMaterialTexureAtlasParameters))
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

static bool LoadGravityAssetsFromDirectory(const FString& RootDirectory, TArray<FGravityImporterListViewRowInfoPtr>& OutAssetInfos)
{
	if (RootDirectory.IsEmpty())
	{
		return false;
	}

	UE_LOG(LogGravityAssetImporter, Display, TEXT("Loading asset from directory: '%s'."), *RootDirectory);

	IFileManager& fileManager = FFileManagerGeneric::Get();

	if (fileManager.DirectoryExists(*RootDirectory))
	{
		TArray<FString> childDirectories;

		fileManager.IterateDirectory(*RootDirectory,
			[&childDirectories](const TCHAR* FileName, bool bIsDirectory)
		{
			if (bIsDirectory)
			{
				childDirectories.Emplace(FileName);
			}

			return true;
		});

		for (const auto& directoryPath : childDirectories)
		{
			FGravityAssetImporterAssetInfoPtr gravityAssetInfo = LoadGravityAssetInfoFromDirectory(directoryPath);

			if (gravityAssetInfo)
			{
				FString fbxFilePath = FPaths::Combine(directoryPath, gravityAssetInfo->Name + ".fbx");

				if (fileManager.FileExists(*fbxFilePath))
				{
					FGravityImporterListViewRowInfoPtr gravityAssetImportInfo = MakeShared<FGravityImporterListViewRowInfo>();

					gravityAssetImportInfo->AssetInfo = gravityAssetInfo;
					gravityAssetImportInfo->AssetFilePath = fbxFilePath;
					gravityAssetImportInfo->bIsMarkedForImport = true;

					OutAssetInfos.Emplace(gravityAssetImportInfo);
				}
				else
				{
					UE_LOG(LogGravityAssetImporter, Warning, TEXT("The asset directory '%s' is missing the fbx file '%s'."), *directoryPath, *fbxFilePath);
				}
			}
		}

		if (OutAssetInfos.Num() == 0)
		{
			UE_LOG(LogGravityAssetImporter, Warning, TEXT("The specified root asset directory does not contain any assets."), *RootDirectory);

			return false;
		}
	}
	else
	{
		UE_LOG(LogGravityAssetImporter, Warning, TEXT("Failed to open specified root asset directory."), *RootDirectory);

		return false;
	}

	return true;
}

SGravityAssetImporter::SGravityAssetImporter()
{
	FAssetToolsModule& assetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");

	AssetTools = &(assetToolsModule.Get());

	FAssetRegistryModule& assetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	AssetRegistry = &(assetRegistryModule.Get());
}

SGravityAssetImporter::~SGravityAssetImporter()
{
	Arguments->SaveEditorConfig();
}

void SGravityAssetImporter::RebuildAssetListView()
{
	TArray<FGravityImporterListViewRowInfoPtr> gravityAssetImportInfos;

	if (LoadGravityAssetsFromDirectory(Arguments->AssetRootDir.Path, gravityAssetImportInfos))
	{
		AssetListView->GravityAssetImportInfos = MoveTemp(gravityAssetImportInfos);

		AssetListView->RebuildList();
	}
}

void SGravityAssetImporter::Construct(const FArguments& Args)
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	Arguments = TStrongObjectPtr<UGravityAssetImporterArguments>(NewObject<UGravityAssetImporterArguments>());
	Arguments->LoadEditorConfig();

	FDetailsViewArgs detailsViewArgs;
	detailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	detailsViewArgs.bAllowSearch = false;
	detailsViewArgs.bShowOptions = true;
	detailsViewArgs.bShowModifiedPropertiesOption = true;
	detailsViewArgs.NotifyHook = this;

	ArgumentsDetailsView = PropertyEditorModule.CreateDetailView(detailsViewArgs);
	ArgumentsDetailsView->SetObject(Arguments.Get());

	FMargin padding(5.0f);

	AssetListView = SNew(SGravityAssetImporterListView);
	
	RebuildAssetListView();

	ChildSlot
	[
		SNew(SVerticalBox)

		// slot for the main tool view
		+ SVerticalBox::Slot()
		.Padding(padding)
		[
			SNew(SSplitter)
			.Orientation(EOrientation::Orient_Horizontal)
			
			// slot for arguments
			+ SSplitter::Slot()
			.Value(0.4f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				.Padding(padding)
				[
					ArgumentsDetailsView->AsShared()
				]
			]

			// slot for listing loaded content
			+ SSplitter::Slot()
			.Value(0.6f)
			[
				AssetListView->AsShared()
			]
		]
		
		// slot for the buttons
		+ SVerticalBox::Slot()
		.Padding(padding)
		.AutoHeight()
		.HAlign(HAlign_Right)
		[
			SNew(SButton)
			.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
			.ButtonStyle(FAppStyle::Get(), "FlatButton.Success")
			.ForegroundColor(FLinearColor::White)
			.ToolTipText(LOCTEXT("TooltipText", "Import assets into project"))
			.Text(LOCTEXT("ImportButtonText", "Import"))
			.OnClicked(this, &SGravityAssetImporter::OnImportClicked)
		]
	];
}

void SGravityAssetImporter::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged)
{
	if (!PropertyChangedEvent.MemberProperty)
	{
		return;
	}

	if (PropertyChangedEvent.MemberProperty->GetName() == TEXT("AssetRootDir"))
	{
		RebuildAssetListView();
	}
}

FReply SGravityAssetImporter::OnImportClicked()
{	
	ImportMeshes();

	return FReply::Unhandled();
}

void SGravityAssetImporter::ImportMeshes()
{
	// @todo - make this an importer option.
	const bool bSkipImportedAssets = true;
	const bool bSavePackages = true;

	UFbxImportUI* fbxImportUI = NewObject<UFbxImportUI>();

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

	TObjectPtr<UFbxFactory> fbxFactory = UFbxFactory::StaticClass()->GetDefaultObject<UFbxFactory>();

	TArray<TObjectPtr<UAssetImportTask>> assetImportTasks;
	TArray<FGravityAssetImporterAssetInfoPtr> gravityAssetInfos;

	for (const auto& assetImportInfo : AssetListView->GravityAssetImportInfos)
	{
		FString meshFilename = assetImportInfo->AssetInfo->Name;
		meshFilename.ToLowerInline();

		FString outputMeshContentDir = FPaths::Combine(Arguments->OutputContentDir.Path, meshFilename);

		// just check if the target import directory contains assets
		const bool bShouldSkipThisAsset = bSkipImportedAssets && AssetRegistry->HasAssets(*outputMeshContentDir);

		if (assetImportInfo->bIsMarkedForImport && !bShouldSkipThisAsset)
		{
			TObjectPtr<UAssetImportTask> assetImportTask = NewObject<UAssetImportTask>();
			assetImportTask->bAutomated = true;
			assetImportTask->Options = fbxImportUI;
			assetImportTask->bReplaceExisting = true;
			assetImportTask->bSave = false;
			assetImportTask->Filename = assetImportInfo->AssetFilePath;
			assetImportTask->DestinationPath = outputMeshContentDir;
			assetImportTask->Factory = fbxFactory;
			assetImportTask->DestinationName = TEXT("SM"); // static mesh prefix

			assetImportTask->AddToRoot();

			assetImportTasks.Emplace(assetImportTask);

			gravityAssetInfos.Emplace(assetImportInfo->AssetInfo);
		}
	}

	FScopedSlowTask SlowTask(assetImportTasks.Num(), LOCTEXT("ModifySlowTask", "Modify"));
	SlowTask.MakeDialog();

	uint32 numCompletedImportsSinceLastGC = 0;

	for (int32 i = 0; i < assetImportTasks.Num(); ++i)
	{
		TObjectPtr<UAssetImportTask>& assetImportTask = assetImportTasks[i];

		AssetTools->ImportAssetTasks({ assetImportTask });

		FGravityAssetImporterAssetInfoPtr gravityAssetInfo = gravityAssetInfos[i];

		const FString& assetFilename = FPaths::GetBaseFilename(assetImportTask->Filename);

		SlowTask.EnterProgressFrame(1, FText::Format(LOCTEXT("Modify_ModifyingAsset", "Modifying \"{0}\"..."), FText::FromString(assetFilename)));

		const TArray<UObject*>& importedObjects = assetImportTask->GetObjects();

		UStaticMesh* importedStaticMesh = nullptr;

		for (UObject* importedObject : importedObjects)
		{
			importedStaticMesh = Cast<UStaticMesh>(importedObject);

			CreateMaterials(importedStaticMesh, gravityAssetInfo->MaterialInfos);

			ModifyImportedStaticMesh(importedStaticMesh);
		}

		++numCompletedImportsSinceLastGC;

		assetImportTask->RemoveFromRoot();
		assetImportTask = nullptr;

		// cleanup after performing a number of imports
		if (numCompletedImportsSinceLastGC == 16 || !assetImportTasks.IsValidIndex(i + 1))
		{
			fbxFactory->CleanUp();

			// force garbace collection so we able to load new objects.
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

			numCompletedImportsSinceLastGC = 0;

			if (bSavePackages)
			{
				FEditorFileUtils::SaveDirtyPackages(false, false, true);
			}
		}
	}
}

void SGravityAssetImporter::ModifyImportedStaticMesh(UStaticMesh* StaticMesh)
{
	StaticMesh->Modify();
	StaticMesh->NaniteSettings.bEnabled = true;
	StaticMesh->PostEditChange();
}

void SGravityAssetImporter::CreateMaterials(UStaticMesh* StaticMesh, const TMap<FString, FGravityAssetImporterMaterialInfo>& MaterialInfos)
{
	const TArray<FStaticMaterial> staticMaterials = StaticMesh->GetStaticMaterials();

	for (const auto& staticMaterial : staticMaterials)
	{
		FString materialSlotName = staticMaterial.MaterialSlotName.ToString();
		const FGravityAssetImporterMaterialInfo* gravityMaterialInfo = MaterialInfos.Find(materialSlotName);

		if (gravityMaterialInfo)
		{

		}
		else
		{
			UE_LOG(LogGravityAssetImporter, Warning, TEXT("Could not find material info for material slot '%s' of mesh '%s'."), *materialSlotName, *(StaticMesh->GetName()));
		}
	}
}

UMaterialInstance* SGravityAssetImporter::GetOrCreateMaterialInstance(const FGravityAssetImporterMaterialInfo& MaterialInfo)
{
	const FString* registeredBaseMaterialFilePath = MaterialRegistry.Find(MaterialInfo.Name);

	if (registeredBaseMaterialFilePath)
	{
		return LoadObject<UMaterialInstance>(nullptr, **registeredBaseMaterialFilePath, nullptr, LOAD_EditorOnly, nullptr);
	}

	// determine the type of base material and load it
	FString baseMaterialFilePath = FString::Format(TEXT("Material'{0}/M_Type{1}.M_Type{1}'"), { Arguments->BaseMaterialDir.Path, MaterialInfo.Type });

	UMaterial* baseMaterial = LoadObject<UMaterial>(nullptr, *baseMaterialFilePath, nullptr, LOAD_EditorOnly, nullptr);

	//if (!baseMaterial)
	//{
	//	UE_LOG(LogGravityAssetImporter, Warning, TEXT("Could not find base material '%s' for material instance '%s'."), *
	//}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE