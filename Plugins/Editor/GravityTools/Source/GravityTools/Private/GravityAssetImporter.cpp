#include "GravityAssetImporter.h"

#include "GravityAssetImporterArguments.h"
#include "GravityAssetImporterListView.h"
#include "GravityAssetTools.h"

#include <Widgets/Input/SButton.h>
#include <Widgets/Layout/SScrollBox.h>
#include <Widgets/Layout/SSplitter.h>
#include <PropertyEditorModule.h>
#include <DetailsViewArgs.h>
#include <HAL/FileManagerGeneric.h>

DEFINE_LOG_CATEGORY(LogGravityAssetImporter)

#define LOCTEXT_NAMESPACE "SGravityAssetImporter"

static TArray<FGravityImporterListViewRowInfoPtr> GenerateListViewRowsFromDirectory(const FString& RootDirectory)
{
	TArray<FGravityImporterListViewRowInfoPtr> listViewRows;

	if (RootDirectory.IsEmpty())
	{
		return listViewRows;
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
			FString failReason;
			if (UGravityAssetTools::IsImportDirectoryValid(directoryPath, failReason))
			{
				FGravityImporterListViewRowInfoPtr gravityAssetImportInfo = MakeShared<FGravityImporterListViewRowInfo>();

				gravityAssetImportInfo->SourceDir = directoryPath;
				gravityAssetImportInfo->AssetName = FPaths::GetBaseFilename(directoryPath);
				gravityAssetImportInfo->bIsMarkedForImport = true;

				listViewRows.Emplace(gravityAssetImportInfo);
			}
			else
			{
				UE_LOG(LogGravityAssetImporter, Warning, TEXT("%s"), *failReason);
			}
		}

		if (listViewRows.Num() == 0)
		{
			UE_LOG(LogGravityAssetImporter, Warning, TEXT("The specified root asset directory does not contain any assets."), *RootDirectory);
		}
	}
	else
	{
		UE_LOG(LogGravityAssetImporter, Warning, TEXT("Failed to open specified root asset directory."), *RootDirectory);
	}

	return listViewRows;
}

SGravityAssetImporter::~SGravityAssetImporter()
{
	Arguments->SaveEditorConfig();
}

void SGravityAssetImporter::RebuildAssetListView()
{
	AssetListView->GravityAssetImportInfos = GenerateListViewRowsFromDirectory(Arguments->SourceMeshDir.Path);

	AssetListView->RebuildList();
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

	if (PropertyChangedEvent.MemberProperty->GetName() == GET_MEMBER_NAME_CHECKED(UGravityAssetImporterArguments, SourceMeshDir))
	{
		RebuildAssetListView();
	}
}

FReply SGravityAssetImporter::OnImportClicked()
{
	TArray<FGravityAssetImportTask> importTasks;

	FGravityAssetImportTask importTask;
	importTask.SourceTextureDir = Arguments->SourceTextureDir.Path;
	importTask.BaseMaterialDir = Arguments->BaseMaterialDir.Path;
	importTask.OutputMeshDir = Arguments->OutputMeshDir.Path;
	importTask.OutputTextureDir = Arguments->OutputTextureDir.Path;

	for (const auto& assetImportInfo : AssetListView->GravityAssetImportInfos)
	{
		if (assetImportInfo->bIsMarkedForImport)
		{
			importTask.SourceMeshDir = assetImportInfo->SourceDir;

			importTasks.Emplace(importTask);
		}
	}

	UGravityAssetTools::ImportAssetTasks(importTasks, Arguments->ImportBatchSize, Arguments->bSavePackages);

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE