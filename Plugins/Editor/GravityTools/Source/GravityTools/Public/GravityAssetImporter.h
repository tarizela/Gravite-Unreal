#pragma once

#include <CoreMinimal.h>
#include <Widgets/SCompoundWidget.h>
#include <Misc/NotifyHook.h>

DECLARE_LOG_CATEGORY_EXTERN(LogGravityAssetImporter, Display, All)

class IDetailsView;
class UGravityAssetImporterArguments;
class SGravityAssetImporterListView;

class SGravityAssetImporter : public SCompoundWidget, public FNotifyHook
{
	SLATE_BEGIN_ARGS(SGravityAssetImporter)
	{
	}
	SLATE_END_ARGS()

public:
	~SGravityAssetImporter();

	void Construct(const FArguments& Args);

private:
	void RebuildAssetListView();
	FReply OnImportClicked();

	virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged) override;

private:
	TSharedPtr<IDetailsView> ArgumentsDetailsView;
	TSharedPtr<SGravityAssetImporterListView> AssetListView;
	TStrongObjectPtr<UGravityAssetImporterArguments> Arguments;
};