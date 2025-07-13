#pragma once

#include <CoreMinimal.h>
#include <Subsystems/EngineSubsystem.h>
#include <Engine/Texture2DDynamic.h>

#include "CameraFrameGrabberSubsystem.generated.h"

class UMaterialInterface;
class UTextureRenderTarget2D;
class FFrameGrabberSceneViewExtension;

DECLARE_LOG_CATEGORY_EXTERN(LogGravityCameraFrameGrabber, Display, All)

class FMaterialTexture2DResource;

UENUM(BlueprintType)
enum class ECroppingRegionSizingAxis : uint8
{
	Horizontal	UMETA(Tooltip="ROI width is fixed and heigh will be computed."),
	Vertical	UMETA(Tooltip="ROI height is fixed and width will be computed."),
	Diagonal	UMETA(Tooltip="ROI diagonal is fixed width and heigh will be computed.")
};

USTRUCT(BlueprintType)
struct FRegionOfInterest
{
	GENERATED_BODY()

	/** Offset of the cropping (the origin is at the top left corner). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CroppingRegion")
	FVector2D Offset = {};

	/** The size of the cropped region along the sizing axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CroppingRegion")
	double Size = 1.0;

	/** The axis for sizing of the ROI.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CroppingRegion")
	ECroppingRegionSizingAxis SizingAxis = ECroppingRegionSizingAxis::Diagonal;

	/** Returns true if the ROI setup is valid. */
	bool IsValid()
	{
		return Offset.X >= 0.0 && Offset.Y >= 0.0 && Size > DOUBLE_KINDA_SMALL_NUMBER;
	}
};

UCLASS()
class UMaterialTexture2D final : public UTexture2DDynamic
{
	GENERATED_BODY()

public:
	// UTexture2DDynamic Interface
	virtual FTextureResource* CreateResource() override;

	/** Updates the underlying RHI resource. */
	void UpdateTextureResource(FRHITexture* InTextureRHI);

	/** Creates a new texture that can be forwarded to material instances. */
	static UMaterialTexture2D* Create();

private:
	/** Reference to a texture resource that was created outside of this class. */
	FTextureRHIRef ExternalTextureRHI;
};

UCLASS(MinimalAPI)
class UCameraFrameGrabberSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	/** UGameInstanceSubsystem Interface */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Set the texture target. The grabbed and processed frame will be written to this target. */
	UFUNCTION(BlueprintSetter, Category=CameraFrameGrabber)
	void SetTextureTarget(UTextureRenderTarget2D* InTextureTarget);

	/** Set the material for post-processing. The subsystem will not run any post-processing if no material is set. */
	UFUNCTION(BlueprintSetter, Category = CameraFrameGrabber)
	void SetPostProcessMaterial(UMaterial* InPostProcessMaterial);

	/** Set parameters for the croping of the grabbed frame. */
	UFUNCTION(BlueprintSetter, Category = CameraFrameGrabber)
	void SetCroppingParameters(const FRegionOfInterest& InParameters);

	/** Set to true if you want the subsysttem to generate mip maps for the grabbed frame. Generating mipmaps cost some performance but lead to better image quality. */
	UFUNCTION(BlueprintSetter, Category = CameraFrameGrabber)
	void SetGenerateInputMips(bool bShouldGenerateInputMips);

	/** Grabs the rendered image at the end of the current frame. */
	UFUNCTION(BlueprintCallable, Category = CameraFrameGrabber)
	void GrabFrame();

protected:
	/** Render target for storting the grabbed and post-processed frame. */
	UPROPERTY(BlueprintSetter=SetTextureTarget)
	TObjectPtr<UTextureRenderTarget2D> TextureTarget;

	/** Material which is used to post-process the grabbed frame. */
	UPROPERTY(BlueprintSetter=SetPostProcessMaterial)
	TObjectPtr<UMaterial> PostProcessMaterial;

	/** Specify the parameters (in pixels) for the cropping of the grabbed frame. */
	UPROPERTY(BlueprintSetter= SetCroppingParameters)
	FRegionOfInterest CroppingParameters;

	/** 
	 * Set to true to generate mip-maps for the grabbed texture target.
	 * This will improve the image quality when rendering the grabbed frame to a small texture target.
	 * This parameter is only relevant when a post-process material is active.
	 */
	UPROPERTY(BlueprintSetter=SetGenerateInputMips)
	bool bGenerateInputMips = false;

private:
	/** Extension that will perform the cropping and post-processing of the grabbed frame. */
	TSharedPtr<FFrameGrabberSceneViewExtension> SceneViewExtension;

	/** Instance of the post-process material that will receive the grabbed frame for processing. */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> PostPocessorMaterialInstance;
};