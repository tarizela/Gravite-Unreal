#include "MobilitySetter.h"

#include <WorldPartition/DataLayer/DataLayerAsset.h>
#include <WorldPartition/DataLayer/DataLayerManager.h>
#include <WorldPartition/DataLayer/DataLayerInstance.h>
#include <Components/InstancedStaticMeshComponent.h>

AMobilitySetter::AMobilitySetter()
{
	UStaticMeshComponent* staticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComponent"));

	ConstructorHelpers::FObjectFinder<UStaticMesh> staticMesh(TEXT("StaticMesh'/Engine/BasicShapes/Sphere.Sphere'"));

	staticMeshComponent->SetStaticMesh(staticMesh.Object);

	staticMeshComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
}

void AMobilitySetter::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.MemberProperty)
	{
		if (PropertyChangedEvent.MemberProperty->GetName() == GET_MEMBER_NAME_CHECKED(AMobilitySetter, bSetMobility))
		{
			UWorld* world = GetWorld();

			TArray<UPrimitiveComponent*> actorComponents;

			for (auto& level : world->GetLevels())
			{
				bool bLevelWasModified = false;

				for (auto& actor : level->Actors)
				{
					if (actor)
					{
						actor->GetComponents(actorComponents);

						for (auto& component : actorComponents)
						{
							component->Mobility = EComponentMobility::Static;

							bLevelWasModified = true;
						}
					}
				}

				if (bLevelWasModified)
				{
					level->Modify();
				}
			}

			bSetMobility = false;

			return;
		}

		if (PropertyChangedEvent.MemberProperty->GetName() == GET_MEMBER_NAME_CHECKED(AMobilitySetter, bClearDataLayers))
		{
			UWorld* world = GetWorld();

			for (auto& level : world->GetLevels())
			{
				bool bLevelWasModified = false;

				for (auto& actor : level->Actors)
				{
					if (actor)
					{
						const auto removedDataLayers = actor->RemoveAllDataLayers();

						if (removedDataLayers.IsEmpty())
						{
							bLevelWasModified = true;
						}
					}
				}
			}

			bClearDataLayers = false;
		}

		if (PropertyChangedEvent.MemberProperty->GetName() == GET_MEMBER_NAME_CHECKED(AMobilitySetter, bAssingDataLayers))
		{
			UWorld* world = GetWorld();

			for (auto& level : world->GetLevels())
			{
				bool bLevelWasModified = false;

				for (auto& actor : level->Actors)
				{
					if (actor)
					{
						TArray<UInstancedStaticMeshComponent*> actorComponents;

						actor->GetComponents(actorComponents);

						UDataLayerAsset* dataLayerAsset = nullptr;

						for (const auto& component : actorComponents)
						{
							UStaticMesh* mesh = component->GetStaticMesh();

							if (mesh)
							{
								FBoxSphereBounds meshBounds = mesh->GetBounds();

								for (const auto& info : DataLayerAssignmentInfos)
								{
									if (meshBounds.SphereRadius >= info.MeshRadius)
									{
										dataLayerAsset = info.DataLayerAsset;
									}
								}
							}
						}

						const auto removedDataLayers = actor->RemoveAllDataLayers();

						if (!removedDataLayers.IsEmpty())
						{
							bLevelWasModified = true;
						}

						if (dataLayerAsset)
						{
							UDataLayerManager* DataLayerManager = UDataLayerManager::GetDataLayerManager(actor.Get());

							if (DataLayerManager)
							{
								const UDataLayerInstance* dataLayerInstance = nullptr;

								dataLayerInstance = DataLayerManager->GetDataLayerInstanceFromAsset(dataLayerAsset);

								if (dataLayerInstance && actor->AddDataLayer(dataLayerInstance))
								{
									bLevelWasModified = true;
								}
							}
						}
					}
				}

				if (bLevelWasModified)
				{
					level->Modify();
				}
			}

			bAssingDataLayers = false;
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}