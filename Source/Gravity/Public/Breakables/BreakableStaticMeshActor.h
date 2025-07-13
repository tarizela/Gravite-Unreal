#pragma once

#include "Breakables/BreakableActorInterface.h"

#include "BreakableStaticMeshActor.generated.h"

class UStaticMeshComponent;

UCLASS(ClassGroup=Breakable, Blueprintable, MinimalAPI)
class ABreakableStaticMeshActor final : public ABreakableActorInterface
{
	GENERATED_BODY()

public:
	ABreakableStaticMeshActor();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/**
	 * Breaks the actor on command.
	 * @param bForce If set to true then the actor will break even if bIsBreakable is disabled.
	 * @returns True if the actor did break.
	 */
	UFUNCTION(BlueprintCallable, Category = "Breakable")
	bool BreakActor(bool bForce = false);

	/**
	 * @returns True if the actor is broken.
	 */
	UFUNCTION(BlueprintCallable, Category = "Breakable")
	bool IsActorBroken() const { return bIsActorBroken; }

private:
	FActorBreakResult OnBreakActor(UPrimitiveComponent* MyComponent, UPrimitiveComponent* OtherComponent, const FHitResult& Hit) override;

	FActorBreakResult BreakActorInternal(bool bForce);

private:
	/**
	 * The static mesh component for rendering the object and receive hit events.
	 */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> StaticMeshComponent;

	/**
	 * Is set to true if the actor has been broken.
	 */
	UPROPERTY()
	bool bIsActorBroken;
};