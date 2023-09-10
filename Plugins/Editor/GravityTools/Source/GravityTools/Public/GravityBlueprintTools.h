#pragma once

#include "GravityBlueprintTools.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogGravityBlueprintTools, Display, All)

UCLASS(BlueprintType, Blueprintable, Transient)
class GRAVITYTOOLS_API UGravityBlueprintTools : public UObject
{
	GENERATED_BODY()

public:

	/**
	 * Creates a new blueprint asset with the relevant GR assets.
	 */
	UFUNCTION(BlueprintCallable)
	static void CreateBlueprintForMesh(const FString& MeshAssetPath);
};