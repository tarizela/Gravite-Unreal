#include "Breakables/DebrisStaticMeshComponent.h"

#include <PhysicsEngine/BodyInstance.h>

UDebrisStaticMeshComponent::UDebrisStaticMeshComponent()
	: DebrisLifetime(5.0f)
	, DebrisDespawnDuration(2.0f)
	, bIsDispawning(false)
	, bIsDebrisAlife(true)
{
	DebrisStartDespawnTimeout = DebrisLifetime;
	DebrisDespawnTimeout = DebrisDespawnDuration;

	PrimaryComponentTick.bCanEverTick = true;

	SetSimulatePhysics(true);
}

void UDebrisStaticMeshComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsDispawning)
	{
		if (!IsAnyRigidBodyAwake())
		{
			DebrisStartDespawnTimeout -= DeltaTime;

			if (DebrisStartDespawnTimeout <= 0)
			{
				OnBeginDespawn();
			}
		}
		else
		{
			// we have to reset the timeout because the rigid body was moved
			DebrisStartDespawnTimeout = DebrisLifetime;

			AActor* debrisActor = GetOwner();
			checkf(debrisActor, TEXT("UDebrisStaticMeshComponent must be owned by ADebrisStaticMeshActor."));

			if (!CheckStillInWorld())
			{
				// we are not in the world anymore, despawn the debris immediately
				DespawnImmediately();
			}
		}
	}
	else
	{
		DebrisDespawnTimeout -= DeltaTime;

		if (DebrisDespawnTimeout <= 0)
		{
			OnEndDespawn();
		}
	}
}

void UDebrisStaticMeshComponent::OnBeginDespawn()
{
	bIsDispawning = true;

	SetSimulatePhysics(false);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void UDebrisStaticMeshComponent::OnEndDespawn()
{
	SetVisibility(false, false);
	SetComponentTickEnabled(false);

	bIsDebrisAlife = false;
	bIsDispawning = false;
}

void UDebrisStaticMeshComponent::DespawnImmediately()
{
	if (bIsDebrisAlife)
	{
		OnBeginDespawn();
		OnEndDespawn();
	}
}

bool UDebrisStaticMeshComponent::CheckStillInWorld()
{
	const UWorld* myWorld = GetWorld();
	if (!myWorld)
	{
		return false;
	}

	AWorldSettings* worldSettings = myWorld->GetWorldSettings(true);
	if (!worldSettings->AreWorldBoundsChecksEnabled())
	{
		return true;
	}

	AActor* actorOwner = GetOwner();
	if (!IsValid(actorOwner))
	{
		return false;
	}

	FVector componentLocation = GetComponentLocation();

	if (componentLocation.Z < worldSettings->KillZ)
	{
		return false;
	}
	else if (IsRegistered())
	{
		const FBox& Box = Bounds.GetBox();
		if (Box.Min.X < -HALF_WORLD_MAX || Box.Max.X > HALF_WORLD_MAX ||
			Box.Min.Y < -HALF_WORLD_MAX || Box.Max.Y > HALF_WORLD_MAX ||
			Box.Min.Z < -HALF_WORLD_MAX || Box.Max.Z > HALF_WORLD_MAX)
		{
			return false;
		}
	}

	return true;
}