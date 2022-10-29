#pragma once

#include <CoreMinimal.h>
#include <Modules/ModuleManager.h>

class SDockTab;
class FSpawnTabArgs;

class FGravityToolsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TSharedRef<SDockTab> SpawnGravityAssetImporterTab(const FSpawnTabArgs& SpawnTabArgs);
};
