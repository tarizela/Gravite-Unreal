#include "Breakables/DebrisStaticMeshActor.h"

#include <Components/SceneComponent.h>

#include "Breakables/DebrisStaticMeshComponent.h"

ADebrisStaticMeshActor::ADebrisStaticMeshActor()
{
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("DebrisStaticMeshActor_RootComponent"));

	RootComponent->SetMobility(EComponentMobility::Movable);
}

void ADebrisStaticMeshActor::TickActor(float DeltaTime, ELevelTick TickType, FActorTickFunction& ThisTickFunction)
{
	Super::TickActor(DeltaTime, TickType, ThisTickFunction);

	bool bHasAlifeDebrisComponents = false;

	for (const UDebrisStaticMeshComponent* debrisComponent : DebrisStaticMeshComponents)
	{
		if (debrisComponent->IsDebrisAlife())
		{
			bHasAlifeDebrisComponents = true;
			break;
		}
	}

	if (!bHasAlifeDebrisComponents)
	{
		Destroy();
	}
}

UDebrisStaticMeshComponent* ADebrisStaticMeshActor::AddDebris()
{
	UDebrisStaticMeshComponent* debrisComponent = NewObject<UDebrisStaticMeshComponent>(this);

	// note that the physics system will detach the debris component from the root because physics objects cannot be attached (makes no sense).
	debrisComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);

	DebrisStaticMeshComponents.Add(debrisComponent);

	return debrisComponent;
}