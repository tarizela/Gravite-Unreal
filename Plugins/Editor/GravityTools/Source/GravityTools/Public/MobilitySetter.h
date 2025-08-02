#pragma once

#include <GameFramework/Actor.h>

#include "MobilitySetter.generated.h"

class UDataLayerAsset;

USTRUCT()
struct FDataLayerAssignmentInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	double MeshRadius = 1000.0f;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UDataLayerAsset> DataLayerAsset;
};

UCLASS()
class GRAVITYTOOLS_API AMobilitySetter : public AActor
{
	GENERATED_BODY()

public:
	AMobilitySetter();

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

private:
	UPROPERTY(EditAnywhere)
	bool bSetMobility = false;

	UPROPERTY(EditAnywhere)
	bool bAssingDataLayers = false;

	UPROPERTY(EditAnywhere)
	bool bClearDataLayers = false;

	UPROPERTY(EditAnywhere)
	TArray<FDataLayerAssignmentInfo> DataLayerAssignmentInfos;
};