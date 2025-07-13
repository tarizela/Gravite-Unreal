#pragma once

#include <GameFramework/Actor.h>

#include "DebrisStaticMeshActor.generated.h"

class UDebrisStaticMeshComponent;

UCLASS(ClassGroup=Breakable, Blueprintable, MinimalAPI)
class ADebrisStaticMeshActor : public AActor
{
	GENERATED_BODY()

public:
	ADebrisStaticMeshActor();

	virtual void TickActor(float DeltaTime, ELevelTick TickType, FActorTickFunction& ThisTickFunction) override;

	/**
	 * Creates a debris object with default parameters.
	 */
	UDebrisStaticMeshComponent* AddDebris();

private:
	UPROPERTY()
	TArray<TObjectPtr<UDebrisStaticMeshComponent>> DebrisStaticMeshComponents;
};