#pragma once

#include <CoreMinimal.h>
#include <Widgets/Views/SListView.h>

#include "GravityAssetImporter.h"

struct FGravityImporterListViewRowInfo
{
	FGravityAssetImporterAssetInfoPtr AssetInfo;

	FString AssetFilePath;

	bool bIsMarkedForImport;
};

using FGravityImporterListViewRowInfoPtr = TSharedPtr<FGravityImporterListViewRowInfo>;

class SGravityAssetImporterListView : public SListView<FGravityImporterListViewRowInfoPtr>
{
	SLATE_BEGIN_ARGS(SGravityAssetImporterListView)
	{
	}
	SLATE_END_ARGS()
	
public:
	void Construct(const FArguments& Args);

	TSharedRef<ITableRow> OnGenerateRow(FGravityImporterListViewRowInfoPtr Item, const TSharedRef<STableViewBase>& OwnerTable);

	void OnToggleMarkAllAssetsForImport(ECheckBoxState CheckState);

public:
	TArray<FGravityImporterListViewRowInfoPtr> GravityAssetImportInfos;
};