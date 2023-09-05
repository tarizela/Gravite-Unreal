#include "GravityAssetImporterListView.h"

#include "Widgets/Input/SCheckBox.h"

#define LOCTEXT_NAMESPACE "SGravityAssetImporterListView"

namespace GravityAssetImporterListViewID
{
	static const FName HeaderColumnCheckBox = TEXT("HeaderColumnCheckBoxID");
	static const FName HeaderColumnAssetName = TEXT("HeaderColumnAssetNameID");
}

class SGravityImporterListViewRow : public SMultiColumnTableRow<FGravityImporterListViewRowInfoPtr>
{
	SLATE_BEGIN_ARGS(SGravityImporterListViewRow)
		: _RowInfo(nullptr)
	{
	}

	SLATE_ARGUMENT(FGravityImporterListViewRowInfoPtr, RowInfo)
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		RowInfo = Args._RowInfo;

		SMultiColumnTableRow<FGravityImporterListViewRowInfoPtr>::Construct
		(
			FSuperRowType::FArguments()
			.Style(FAppStyle::Get(), "DataTableEditor.CellListViewRow"),
			InOwnerTableView
		);
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		if (ColumnName == GravityAssetImporterListViewID::HeaderColumnCheckBox)
		{
			return SNew(SBox)
				.HAlign(HAlign_Center)
				[
					SNew(SCheckBox)
					.HAlign(EHorizontalAlignment::HAlign_Center)
					.IsChecked(this, &SGravityImporterListViewRow::IsAssetMarkedForImport)
					.OnCheckStateChanged(this, &SGravityImporterListViewRow::OnMarkAssetForImport)
				];
		}
		else if (ColumnName == GravityAssetImporterListViewID::HeaderColumnAssetName)
		{
			return SNew(STextBlock)
				.Text(FText::FromString(RowInfo->AssetInfo->Name))
				.ToolTipText(FText::FromString(RowInfo->AssetInfo->Name));
		}

		return SNullWidget::NullWidget;
	}

private:
	ECheckBoxState IsAssetMarkedForImport() const
	{
		return RowInfo->bIsMarkedForImport ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	void OnMarkAssetForImport(ECheckBoxState CheckState)
	{
		RowInfo->bIsMarkedForImport = CheckState == ECheckBoxState::Checked;
	}

private:
	FGravityImporterListViewRowInfoPtr RowInfo;
};

void SGravityAssetImporterListView::Construct(const FArguments& Args)
{
	SListView::Construct
	(
		SListView::FArguments()
		.ListItemsSource(&GravityAssetImportInfos)
		.SelectionMode(ESelectionMode::Multi)
		.OnGenerateRow(this, &SGravityAssetImporterListView::OnGenerateRow)
		.HeaderRow
		(
			SNew(SHeaderRow)

			+ SHeaderRow::Column(GravityAssetImporterListViewID::HeaderColumnCheckBox)
			.FixedWidth(26)
			.DefaultLabel(FText::GetEmpty())
			[
				SNew(SCheckBox)
				.HAlign(EHorizontalAlignment::HAlign_Center)
				.OnCheckStateChanged(this, &SGravityAssetImporterListView::OnToggleMarkAllAssetsForImport)
			]

			+ SHeaderRow::Column(GravityAssetImporterListViewID::HeaderColumnAssetName)
			.HAlignCell(EHorizontalAlignment::HAlign_Left)
			.DefaultLabel(LOCTEXT("AssetNameHeaderName", "Asset Name"))
		)
	);
}

TSharedRef<ITableRow> SGravityAssetImporterListView::OnGenerateRow(FGravityImporterListViewRowInfoPtr Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SGravityImporterListViewRow, OwnerTable)
		.RowInfo(Item);
}

void SGravityAssetImporterListView::OnToggleMarkAllAssetsForImport(ECheckBoxState CheckState)
{
	for (auto& assetImportInfo : GravityAssetImportInfos)
	{
		assetImportInfo->bIsMarkedForImport = CheckState == ECheckBoxState::Checked;
	}
}

#undef LOCTEXT_NAMESPACE