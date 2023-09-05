#include "GravityToolsModule.h"

#include "GravityAssetImporter.h"

#include <Framework/Docking/TabManager.h>
#include <Widgets/Docking/SDockTab.h>
#include <Textures/SlateIcon.h>
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructure.h"
#include <Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h>

#define LOCTEXT_NAMESPACE "FGravityToolsModule"

namespace GravityToolID
{
	static const FName GravityAssetImporter = TEXT("GravityAssetImporterID");
}

void FGravityToolsModule::StartupModule()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(GravityToolID::GravityAssetImporter, FOnSpawnTab::CreateRaw(this, &FGravityToolsModule::SpawnGravityAssetImporterTab))
		.SetDisplayName(LOCTEXT("TabTitle", "Gravity Asset Importer"))
		.SetTooltipText(LOCTEXT("TooltipText", "Tool for importing GR2 assets which were exported via the GR2 blender exporter script"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("ClassIcon.UserDefinedStruct")));
}

void FGravityToolsModule::ShutdownModule()
{
	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(GravityToolID::GravityAssetImporter);
	}
}

TSharedRef<SDockTab> FGravityToolsModule::SpawnGravityAssetImporterTab(const FSpawnTabArgs& SpawnTabArgs)
{
	TSharedPtr<SDockTab> majorTab;
	SAssignNew(majorTab, SDockTab).TabRole(ETabRole::MajorTab);

	majorTab->SetContent(SNew(SGravityAssetImporter));

	return majorTab.ToSharedRef();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FGravityToolsModule, GravityTools)