#include "Breakables/BreakableStaticMeshActor.h"

#include <Components/StaticMeshComponent.h>

ABreakableStaticMeshActor::ABreakableStaticMeshActor()
	: bIsActorBroken(false)
{
	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BreakableStaticMeshActor_StaticMeshComponent"));

	StaticMeshComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);

	RootComponent->SetMobility(EComponentMobility::Movable);

	StaticMeshComponent->SetSimulatePhysics(true);
	StaticMeshComponent->SetNotifyRigidBodyCollision(true);
}

FActorBreakResult ABreakableStaticMeshActor::OnBreakActor(UPrimitiveComponent* MyComponent, UPrimitiveComponent* OtherComponent, const FHitResult& Hit)
{
	FActorBreakResult breakResult;

	if (!bIsActorBroken && bIsBreakable)
	{
		FVector componentVelocity = MyComponent->GetComponentVelocity();
		float componentMass = MyComponent->GetMass();

		FVector relativeComponentVelocity(0.0);

		if (OtherComponent && OtherComponent->Mobility == EComponentMobility::Movable)
		{
			relativeComponentVelocity = OtherComponent->GetComponentVelocity() - componentVelocity;
		}
		else
		{
			relativeComponentVelocity = componentVelocity;
		}

		// @fixme this test is too simple and will lead to a lot of invalid break events.
		// this test works because of the transmission of the energy when the objects collide.
		float kineticEnergy = componentMass * 0.5f * relativeComponentVelocity.SquaredLength() * 0.0001f;

		if (kineticEnergy >= BreakThreshold)
		{
			breakResult = BreakActorInternal(false);
		}
	}

	return breakResult;
}

FActorBreakResult ABreakableStaticMeshActor::BreakActorInternal(bool bForce)
{
	FActorBreakResult breakResult;

	if (bIsActorBroken)
	{
		return breakResult;
	}

	if (bIsBreakable || bForce)
	{
		bIsActorBroken = true;

		breakResult.bIsBroken = bIsActorBroken;
		breakResult.DebrisTransform = StaticMeshComponent->GetComponentTransform().GetRelativeTransform(GetActorTransform());

		StaticMeshComponent->SetNotifyRigidBodyCollision(false);

		if (BrokenStaticMesh)
		{
			StaticMeshComponent->SetStaticMesh(BrokenStaticMesh);
		}
		else
		{
			// no broken static mesh was assigned, we do not need this actor anymore because it will not be rendered
			Destroy();
		}
	}

	return breakResult;
}

bool ABreakableStaticMeshActor::BreakActor(bool bForce)
{
	FActorBreakResult breakResult = BreakActorInternal(bForce);

	OnPostBreakActor(breakResult);

	return breakResult.bIsBroken;
}

#if WITH_EDITOR
void ABreakableStaticMeshActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.MemberProperty)
	{
		FName memberPropertyName = PropertyChangedEvent.MemberProperty->GetFName();

		if (memberPropertyName == GET_MEMBER_NAME_CHECKED(ABreakableStaticMeshActor, BaseStaticMesh))
		{
			StaticMeshComponent->SetStaticMesh(BaseStaticMesh);
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR