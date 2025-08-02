#include "Breakables/BreakableActorInterface.h"

#include <AssetRegistry/AssetRegistryModule.h>
#include <PhysicalMaterials/PhysicalMaterial.h>
#include <Modules/ModuleManager.h>

#include "Breakables/DebrisStaticMeshComponent.h"
#include "Breakables/DebrisStaticMeshActor.h"

DEFINE_LOG_CATEGORY(LogGravityBreakableObject)

ABreakableActorInterface::ABreakableActorInterface()
	: BreakThreshold(100.0f)
	, bIsBreakable(true)
	, DebrisLifetime(5.0f)
	, DebrisDespawnDuration(2.0f)
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("BreakableActorInterface_RootComponent"));
}

void ABreakableActorInterface::NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	// notify subsystems about the hit before modifying the hit actor
	Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);

	FActorBreakResult actorBreakResult = OnBreakActor(MyComp, OtherComp, Hit);

	OnPostBreakActor(actorBreakResult);
}

void ABreakableActorInterface::OnSpawnDebris(const FTransform& RelativeTransform)
{
	// @todo: optimize the spawn routine, the current one leads to stuttering.
	FActorSpawnParameters actorSpawnParameters;

	actorSpawnParameters.CustomPreSpawnInitalization = [this](AActor* SpawnedActor)
	{
		ADebrisStaticMeshActor* debrisActor = Cast<ADebrisStaticMeshActor>(SpawnedActor);

		for (const auto& staticMesh : DebrisStaticMeshes)
		{
			if (staticMesh)
			{
				UDebrisStaticMeshComponent* debrisComponent = debrisActor->AddDebris();

				debrisComponent->SetStaticMesh(staticMesh);
				debrisComponent->SetDebrisLifetime(DebrisLifetime);
				debrisComponent->SetDebrisDespawnDuration(DebrisDespawnDuration);

				if (DebrisPhysicalMaterial)
				{
					debrisComponent->SetPhysMaterialOverride(DebrisPhysicalMaterial);
				}
			}
		}
	};

	FTransform actorWorldTransform = RelativeTransform * GetActorTransform();

	GetWorld()->SpawnActor<ADebrisStaticMeshActor>(ADebrisStaticMeshActor::StaticClass(), actorWorldTransform, actorSpawnParameters);
}

void ABreakableActorInterface::OnPostBreakActor(const FActorBreakResult& BreakResult)
{
	if (BreakResult.bIsBroken)
	{
		OnSpawnDebris(BreakResult.DebrisTransform);
	}
}

#if WITH_EDITOR
void ABreakableActorInterface::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	const auto* activeMemberNode = PropertyChangedEvent.PropertyChain.GetActiveMemberNode();

	if (activeMemberNode)
	{
		FName memberPropertyName = activeMemberNode->GetValue()->GetFName();

		if (memberPropertyName == GET_MEMBER_NAME_CHECKED(ABreakableActorInterface, DebrisStaticMeshDir))
		{
			DebrisStaticMeshes.Reset();

			if (!DebrisStaticMeshDir.Path.IsEmpty())
			{
				FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

				FARFilter assetRegistryFilter;
				assetRegistryFilter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());
				assetRegistryFilter.PackagePaths.Add(*DebrisStaticMeshDir.Path);

				TArray<FAssetData> assetData;

				AssetRegistryModule.Get().GetAssets(assetRegistryFilter, assetData);

				for (const FAssetData& asset : assetData)
				{
					UStaticMesh* debrisMesh = Cast<UStaticMesh>(asset.GetAsset());

					if (debrisMesh)
					{
						DebrisStaticMeshes.Add(debrisMesh);
					}
					else
					{
						UE_LOG(LogGravityBreakableObject, Warning, TEXT("Breakable actor '%s' failed to load debris mesh '%s'."), *GetName(), *asset.AssetName.ToString());
					}
				}
			}
		}
	}

	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR