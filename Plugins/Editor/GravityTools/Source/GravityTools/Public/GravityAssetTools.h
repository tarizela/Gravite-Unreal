#pragma once

#include "GravityAssetTools.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogGravityAssetTools, Display, All)

USTRUCT(BlueprintType, Blueprintable)
struct FGravityAssetImportTask
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FString SourceMeshDir;

	UPROPERTY(EditAnywhere)
	FString SourceTextureDir;

	UPROPERTY(EditAnywhere)
	FString BaseMaterialDir;

	UPROPERTY(EditAnywhere)
	FString OutputMeshDir;

	UPROPERTY(EditAnywhere)
	FString OutputTextureDir;
};

UCLASS(BlueprintType, Blueprintable, Transient)
class GRAVITYTOOLS_API UGravityAssetTools : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Check if the directory containing source asset is valid.
	 * @param ImportDir The path of the import directoy.
	 * @returns True if the directory can be used for import.
	 */
	UFUNCTION(BlueprintCallable)
	static bool IsImportDirectoryValid(const FString& ImportDir, FString& OutFailReason);

	/**
	 * Imports gravity assets.
	 * @param ImportTasks Tasks for the import.
	 * @param ImportBatchSize The import is performed in batches. Per default the importer will process 16 tasks then save the imported assets if requiested and do a cleanup.
	 * @param bSavePackages True if the importer should save the imported assets.
	 * @returns The number of imported assets.
	 */
	UFUNCTION(BlueprintCallable)
	static int32 ImportAssetTasks(const TArray<FGravityAssetImportTask>& ImportTasks, int32 ImportBatchSize = 16, bool bSavePackages = false);
};