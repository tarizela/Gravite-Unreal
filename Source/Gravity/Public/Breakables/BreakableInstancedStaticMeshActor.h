#pragma once

#include "Breakables/BreakableActorInterface.h"

#include "BreakableInstancedStaticMeshActor.generated.h"

class UInstancedStaticMeshComponent;

UCLASS(ClassGroup=Breakable, Blueprintable, MinimalAPI)
class ABreakableInstancedStaticMeshActor final : public ABreakableActorInterface
{
	GENERATED_BODY()

public:
	ABreakableInstancedStaticMeshActor();

#if WITH_EDITOR
	void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

	/**
	 * Breaks the actor on command.
	 * @param InstanceIndex The index of the instace to be broken.
	 * @param bForce If is set to true then the instance will break even if bIsBreakable is disabled.
	 * @returns True if the actor did.
	 */
	UFUNCTION(BlueprintCallable, Category = "Breakable")
	bool BreakInstance(int32 InstanceIndex, bool bForce = false);

	/**
	 * Allows you to check if an instance is broken.
	 * @param InstanceIndex The 
	 * @returns True if an instance is broken.
	 */
	UFUNCTION(BlueprintCallable, Category = "Breakable")
	bool IsInstanceBroken(int32 InstanceIndex) const;

private:
	FActorBreakResult OnBreakActor(UPrimitiveComponent* MyComponent, UPrimitiveComponent* OtherComponent, const FHitResult& Hit) override;

	FActorBreakResult BreakInstanceInternal(int32 InstanceIndex, bool bForce);

private:
	/**
	 * The ISM component for rendering the intact object and receive hit events.
	 */
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> BaseISMComponent;

	/**
	 * The ISM component for rendering of the broken object.
	 */
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> BrokenISMComponent;

#if WITH_EDITORONLY_DATA
	/**
	 * Relative transforms of the instances.
	 */
	UPROPERTY(EditAnywhere, Category = "Breakable Instances")
	TArray<FTransform> InstanceTransforms;
#endif
};