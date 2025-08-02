#include "Breakables/BreakableInstancedStaticMeshActor.h"

#include <Components/SceneComponent.h>
#include <Components/InstancedStaticMeshComponent.h>

ABreakableInstancedStaticMeshActor::ABreakableInstancedStaticMeshActor()
{
	BaseISMComponent = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BreakableInstancedStaticMeshActor_BaseISMComponent"));
	BrokenISMComponent = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BreakableInstancedStaticMeshActor_BrokenISMComponent"));

	BaseISMComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	BrokenISMComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);

	BaseISMComponent->SetNotifyRigidBodyCollision(true);

	RootComponent->SetMobility(EComponentMobility::Static);
}

FActorBreakResult ABreakableInstancedStaticMeshActor::OnBreakActor(UPrimitiveComponent* MyComponent, UPrimitiveComponent* OtherComponent, const FHitResult& Hit)
{
	FActorBreakResult breakResult;

	if (MyComponent == BaseISMComponent && bIsBreakable)
	{
		check(OtherComponent && OtherComponent->Mobility == EComponentMobility::Movable);

		float kineticEnergy = 0.0f;

		// @fixme need to figure out how to properly get velocity of the colliding object and the mass of the static component.
		if (OtherComponent && OtherComponent->IsAnySimulatingPhysics())
		{
			float otherComponentMass = OtherComponent->GetMass();
			FVector otherComponentVelocity = OtherComponent->GetComponentVelocity();

			kineticEnergy = otherComponentMass * 0.5f * otherComponentVelocity.SquaredLength() * 0.0001f;
		}

		if (kineticEnergy > BreakThreshold)
		{
			int32 instanceIndex = Hit.MyItem;
			
			breakResult = BreakInstanceInternal(instanceIndex, false);
		}
	}

	return breakResult;
}

FActorBreakResult ABreakableInstancedStaticMeshActor::BreakInstanceInternal(int32 InstanceIndex, bool bForce)
{
	FActorBreakResult breakResult;

	if (IsInstanceBroken(InstanceIndex))
	{
		return breakResult;
	}

	if (bIsBreakable || bForce)
	{
		FTransform instanceTransform;

		// this will replace the base instance with the broken one.
		// @todo test if the replacing leads to stuttering because of the ISM buffer updates.
		if (BrokenStaticMesh)
		{
			BaseISMComponent->GetInstanceTransform(InstanceIndex, instanceTransform);

			BrokenISMComponent->AddInstance(instanceTransform);
		}

		BaseISMComponent->RemoveInstance(InstanceIndex);

		// this probably will almost never happen but we destory the actor if it has no rendering relevance anymore.
		if (BaseISMComponent->GetInstanceCount() == 0 && BrokenISMComponent->GetInstanceCount() == 0)
		{
			Destroy();
		}

		breakResult.bIsBroken = true;
		breakResult.DebrisTransform = instanceTransform;
	}

	return breakResult;
}

bool ABreakableInstancedStaticMeshActor::BreakInstance(int32 InstanceIndex, bool bForce)
{
	FActorBreakResult breakResult = BreakInstanceInternal(InstanceIndex, bForce);

	OnPostBreakActor(breakResult);

	return breakResult.bIsBroken;
}

bool ABreakableInstancedStaticMeshActor::IsInstanceBroken(int32 InstanceIndex) const
{
	// broken instances are removed from the base ISM component
	return !BaseISMComponent->IsValidInstance(InstanceIndex);
}

#if WITH_EDITOR
void ABreakableInstancedStaticMeshActor::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	const auto* activeMemberNode = PropertyChangedEvent.PropertyChain.GetActiveMemberNode();

	if (activeMemberNode)
	{
		FName memberPropertyName = activeMemberNode->GetValue()->GetFName();

		if (memberPropertyName == GET_MEMBER_NAME_CHECKED(ABreakableInstancedStaticMeshActor, BaseStaticMesh))
		{
			BaseISMComponent->SetStaticMesh(BaseStaticMesh);
		}
		else if (memberPropertyName == GET_MEMBER_NAME_CHECKED(ABreakableInstancedStaticMeshActor, BrokenStaticMesh))
		{
			BrokenISMComponent->SetStaticMesh(BrokenStaticMesh);
		}
		else if (memberPropertyName == GET_MEMBER_NAME_CHECKED(ABreakableInstancedStaticMeshActor, InstanceTransforms))
		{
			if (PropertyChangedEvent.ChangeType & (EPropertyChangeType::ArrayAdd | EPropertyChangeType::Duplicate | EPropertyChangeType::ArrayRemove))
			{
				int32 instanceIndex = PropertyChangedEvent.GetArrayIndex(memberPropertyName.ToString());

				if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayRemove)
				{
					BaseISMComponent->RemoveInstance(instanceIndex);
				}
				else
				{
					BaseISMComponent->AddInstance(InstanceTransforms[instanceIndex], false);
				}
			}
			else if (PropertyChangedEvent.ChangeType & (EPropertyChangeType::Interactive | EPropertyChangeType::ValueSet))
			{
				// we batch update all instances because we do not know which transform changed only that there was a change
				BaseISMComponent->BatchUpdateInstancesTransforms(0, InstanceTransforms, false, true);
			}
			else if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayClear)
			{
				BaseISMComponent->ClearInstances();
			}
		}
	}

	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR