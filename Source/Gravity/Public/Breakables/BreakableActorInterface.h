#pragma once

#include <GameFramework/Actor.h>

#include "BreakableActorInterface.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogGravityBreakableObject, Display, All)

class UStaticMesh;
class UPhysicalMaterial;

USTRUCT()
struct FActorBreakResult
{
	GENERATED_BODY()

	/** True if the actor did break. */
	bool bIsBroken = false;

	/** Transform for swapning of debris. */
	FTransform DebrisTransform = FTransform::Identity;
};

UCLASS(ClassGroup=Breakable, Abstract, Blueprintable, MinimalAPI)
class ABreakableActorInterface : public AActor
{
	GENERATED_BODY()

public:
	ABreakableActorInterface();

	void NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit) override;

	/**
	 * @param bInIsBreakable The actor will be allowed to break if this is set to true.
	 */
	UFUNCTION(BlueprintCallable, Category="Breakable")
	void SetIsBreakable(bool bInIsBreakable) { bIsBreakable = bInIsBreakable; }

	/**
	 * @returns True if the actor is allowed to break.
	 */
	UFUNCTION(BlueprintCallable, Category="Breakable")
	bool IsBreakable() const { return bIsBreakable; }

#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

protected:
	/**
	 * Event that is triggered when the actor is broken.
	 * @returns Result of the actor breaking apart.
	 */
	virtual FActorBreakResult OnBreakActor(UPrimitiveComponent* MyComponent, UPrimitiveComponent* OtherComponent, const FHitResult& Hit) PURE_VIRTUAL(ABreakableActorInterface::OnBreakActor, return {}; );

	/**
	 * Spawns debris for an instance.
	 * @param RelativeTransform Transform of the debris spawn point.
	 */
	virtual void OnSpawnDebris(const FTransform& RelativeTransform);

	/**
	 * Peforms post-break work like spawning debris.
	 * @param BreakResult Result of the actor break event.
	 */
	virtual void OnPostBreakActor(const FActorBreakResult& BreakResult);

protected:
	/**
	 * Static mesh representing the intact object.
	 */
	UPROPERTY(EditAnywhere, Category="Breakable")
	TObjectPtr<UStaticMesh> BaseStaticMesh;

	/**
	 * Static mesh representing the broken object.
	 */
	UPROPERTY(EditAnywhere, Category="Breakable")
	TObjectPtr<UStaticMesh> BrokenStaticMesh;

#if WITH_EDITORONLY_DATA
	/**
	 * Path to the directory containing debris static meshes.
     */
	UPROPERTY(EditAnywhere, Category="Breakable", meta = (ContentDir))
	FDirectoryPath DebrisStaticMeshDir;
#endif

	/**
	 * Threshold for the break event.
	 */
	UPROPERTY(EditAnywhere, Category="Breakable")
	float BreakThreshold;

	/**
	 * Set to true if you want the object to never break.
	 */
	UPROPERTY(EditAnywhere, Category="Breakable")
	bool bIsBreakable;

	/**
	 * Collection of static meshes for rendering debris.
	 */
	UPROPERTY(EditAnywhere, Category="Breakable")
	TArray<TObjectPtr<UStaticMesh>> DebrisStaticMeshes;

	/**
	 * Physical material for the debris.
	 */ 
	UPROPERTY(EditAnywhere, Category = "Breakable")
	TObjectPtr<UPhysicalMaterial> DebrisPhysicalMaterial;

	/**
	 * Timeout for the debris to start despawning.
	 */
	UPROPERTY(EditAnywhere, Category="Breakable")
	float DebrisLifetime;

	/**
	 * Duration of the despawn sequence.
	 */
	UPROPERTY(EditAnywhere, Category="Breakable")
	float DebrisDespawnDuration;
};