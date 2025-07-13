#pragma once

#include <Components/StaticMeshComponent.h>

#include "DebrisStaticMeshComponent.generated.h"

UCLASS(ClassGroup=Breakable, Blueprintable, MinimalAPI)
class UDebrisStaticMeshComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	UDebrisStaticMeshComponent();

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	bool IsDebrisAlife() const { return bIsDebrisAlife; }

	void SetDebrisLifetime(float InLifetime) { DebrisLifetime = InLifetime; }
	void SetDebrisDespawnDuration(float InTimeout) { DebrisDespawnDuration = InTimeout; }

	float GetDebrisLifetime() const { return DebrisLifetime; }
	float GetDebrisDespawnDuration() const { return DebrisDespawnDuration; }

private:
	// Called when the component is put to sleep.
	void OnBeginDespawn();
	
	// Called when the component finishes dispawning.
	void OnEndDespawn();

	// Despawns the component immediately.
	void DespawnImmediately();

	// Checks if the debris component is still a part of the world.
	bool CheckStillInWorld();

private:
	// Time of the debris until it despawns after being put to sleep.
	UPROPERTY()
	float DebrisLifetime;

	// The duration of the despawn sequence.
	UPROPERTY()
	float DebrisDespawnDuration;

	// Timeout until this component begins despawning.
	float DebrisStartDespawnTimeout;

	// Timeout until this component becomes inactive.
	float DebrisDespawnTimeout;

	// True if the component is in the process of dispawning.
	bool bIsDispawning;

	// True if the debris is still being simulated and rendered.
	bool bIsDebrisAlife;
};