#include "Subsystems/CameraFrameGrabberSubsystem.h"

#include <RHI.h>
#include <SceneViewExtension.h>
#include <RenderGraphUtils.h>
#include <RenderGraphBuilder.h>
#include <MaterialDomain.h>
#include <ScreenPass.h>
#include <CanvasTypes.h>
#include <CanvasItem.h>
#include <Materials/MaterialInstanceDynamic.h>
#include <Engine/TextureRenderTarget2D.h>
#include <DeviceProfiles/DeviceProfile.h>
#include <DeviceProfiles/DeviceProfileManager.h>
#include <GenerateMips.h>

DEFINE_LOG_CATEGORY(LogGravityCameraFrameGrabber)

class FMaterialTexture2DResource final : public FTextureResource
{
public:
	FMaterialTexture2DResource(UMaterialTexture2D* InOwner);

	// FTextureResource Interface
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
	virtual void ReleaseRHI() override;
	virtual uint32 GetSizeX() const override;
	virtual uint32 GetSizeY() const override;

	void UpdateTextureRHI(FRHITexture* InTextureRHI);

private:
	TObjectPtr<UMaterialTexture2D> Owner;
};

FMaterialTexture2DResource::FMaterialTexture2DResource(UMaterialTexture2D* InOwner)
	: Owner(InOwner)
{
}

void FMaterialTexture2DResource::InitRHI(FRHICommandListBase& RHICmdList)
{
	ESamplerAddressMode samplerAdressMode = Owner->SamplerAddressMode;
	FSamplerStateInitializerRHI samplerStateInitializer(
		(ESamplerFilter)UDeviceProfileManager::Get().GetActiveProfile()->GetTextureLODSettings()->GetSamplerFilter(Owner),
		samplerAdressMode,
		samplerAdressMode,
		samplerAdressMode);

	SamplerStateRHI = GetOrCreateSamplerState(samplerStateInitializer);
}

void FMaterialTexture2DResource::ReleaseRHI()
{
	RHIUpdateTextureReference(Owner->TextureReference.TextureReferenceRHI, nullptr);
}

void FMaterialTexture2DResource::UpdateTextureRHI(FRHITexture* InTextureRHI)
{
	RHIUpdateTextureReference(Owner->TextureReference.TextureReferenceRHI, InTextureRHI);
}

uint32 FMaterialTexture2DResource::GetSizeX() const
{
	return Owner->SizeX;
}

uint32 FMaterialTexture2DResource::GetSizeY() const
{
	return Owner->SizeY;
}

FTextureResource* UMaterialTexture2D::CreateResource()
{
	return new FMaterialTexture2DResource(this);
}

void UMaterialTexture2D::UpdateTextureResource(FRHITexture* InTextureRHI)
{
	check(InTextureRHI);

	const FRHITextureDesc& textureDesc = InTextureRHI->GetDesc();

	SizeX = textureDesc.Extent.X;
	SizeY = textureDesc.Extent.Y;
	Format = textureDesc.Format;
	NumMips = textureDesc.NumMips;

	FMaterialTexture2DResource* textureResource = static_cast<FMaterialTexture2DResource*>(GetResource());

	textureResource->UpdateTextureRHI(InTextureRHI);
}

UMaterialTexture2D* UMaterialTexture2D::Create()
{
	UMaterialTexture2D* materialTexture = NewObject<UMaterialTexture2D>(GetTransientPackage(), NAME_None, RF_Transient);

	if (materialTexture)
	{
		// The RHI resource will be set on the render thread.
		materialTexture->Init(0, 0, EPixelFormat::PF_Unknown, false);
	}

	return materialTexture;
}

static ESamplerFilter GetSamplerFilter(UMaterialTexture2D* Texture)
{
	switch (Texture->Filter)
	{
	case TextureFilter::TF_Bilinear:	return ESamplerFilter::SF_Bilinear;
	case TextureFilter::TF_Trilinear:	return ESamplerFilter::SF_Trilinear;
	}

	return ESamplerFilter::SF_Point;
}

class FFrameGrabberSceneViewExtension : public FSceneViewExtensionBase, public FGCObject
{
public:
	FFrameGrabberSceneViewExtension(const FAutoRegister& AutoRegister);

	// FSceneViewExtensionBase Interface
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void PostRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) override;
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;

	// FGCObject Interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const { return TEXT("FrameGrabberSceneViewExtension"); }

	/** Signals the extension to grab the rendering at the end of the frame. */
	void GrabFrame_RenderThread() { bIsActive = true; }

	/** Setter. */
	void SetPostProcessMaterial(UMaterialInstanceDynamic* InPostProcessMaterial);
	void SetGenerateInputMips(bool bInGenerateInputMips);
	void SetTextureTarget(FRHITexture* InTextureTargetRHI)	{ TextureTargetRHI		= InTextureTargetRHI;		}
	void SetCropping(const FRegionOfInterest& InParameters)	{ CroppingParameters	= InParameters;				}

private:
	bool bIsActive = false;

	bool bGenerateInputMips = false;

	FRegionOfInterest CroppingParameters;

	FTextureRHIRef TextureTargetRHI;

	FTextureRHIRef TextureWithMipsRHI;

	TObjectPtr<UMaterialTexture2D> PostProcessTexture;

	TObjectPtr<UMaterialInstanceDynamic> PostProcessMaterial;
};

FFrameGrabberSceneViewExtension::FFrameGrabberSceneViewExtension(const FAutoRegister& AutoRegister)
	: FSceneViewExtensionBase(AutoRegister)
{
	PostProcessTexture = UMaterialTexture2D::Create();

	PostProcessTexture->SamplerAddressMode = ESamplerAddressMode::AM_Clamp;
}

void FFrameGrabberSceneViewExtension::SetPostProcessMaterial(UMaterialInstanceDynamic* InPostProcessMaterial)
{
	if (InPostProcessMaterial)
	{
		check(PostProcessTexture);

		InPostProcessMaterial->SetTextureParameterValue(TEXT("SceneColorTexture"), PostProcessTexture);
	}

	PostProcessMaterial = InPostProcessMaterial;
}

void FFrameGrabberSceneViewExtension::SetGenerateInputMips(bool bInGenerateInputMips)
{
	if (!bInGenerateInputMips)
	{
		// Free up memory if mip maps are not required.
		TextureWithMipsRHI.SafeRelease();
	}

	bGenerateInputMips = bInGenerateInputMips;
}

void FFrameGrabberSceneViewExtension::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (PostProcessMaterial)
	{
		Collector.AddReferencedObject(PostProcessMaterial);
	}

	if (PostProcessTexture)
	{
		Collector.AddReferencedObject(PostProcessTexture);
	}
}

bool FFrameGrabberSceneViewExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	return TextureTargetRHI && bIsActive;
}

void FFrameGrabberSceneViewExtension::PostRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView)
{
	bool bIsPostProcessingActive = PostProcessMaterial != nullptr;
	FIntRect viewRect = InView.UnconstrainedViewRect;

	const FRenderTarget* inputRenderTarget = InView.Family->RenderTarget;

	check(inputRenderTarget);

	FTextureRHIRef inputTextureRHI = inputRenderTarget->GetRenderTargetTexture();

	const FRHITextureDesc& inputTextureDesc = inputTextureRHI->GetDesc();
	const FRHITextureDesc& outputTextureDesc = TextureTargetRHI->GetDesc();

	FIntRect croppingRect;

	// Compute the size of the ROI if required and convert normalized coordinates to absolute.
	croppingRect.Min.X = FMath::RoundToInt32(CroppingParameters.Offset.X > 1.0 ? CroppingParameters.Offset.X : CroppingParameters.Offset.X * viewRect.Width());
	croppingRect.Min.Y = FMath::RoundToInt32(CroppingParameters.Offset.Y > 1.0 ? CroppingParameters.Offset.Y : CroppingParameters.Offset.Y * viewRect.Height());

	// Limit the size of the cropped region to the size of the output texture target.
	if (croppingRect.Min.X > viewRect.Width() || croppingRect.Min.Y > viewRect.Height())
	{
		UE_LOG(LogGravityCameraFrameGrabber, Error, TEXT("Invalid cropping offset was set. The offset must be a coordinate within the bounds of the viewport."));

		// No data to copy. Cancel the operation.
		return;
	}

	// Fit the output texture target area into the viewport.
	FVector2D dstRegionSize = outputTextureDesc.Extent;
	FVector2D srcRegionSize = viewRect.Size() - croppingRect.Min;

	if (srcRegionSize.X < 1 || srcRegionSize.Y < 1)
	{
		UE_LOG(LogGravityCameraFrameGrabber, Warning, TEXT("The cropped area is smaller then one pixel."));

		// No data to copy. Cancel the operation.
		return;
	}

	// This is the size of the ROI with aspect ratio of the target texture.
	dstRegionSize /= FMath::Max(dstRegionSize.X / srcRegionSize.X, dstRegionSize.Y / srcRegionSize.Y);

	// Scale the ROI to the requested size.
	double dstRegionAspect = dstRegionSize.X / dstRegionSize.Y;

	double dstRegionScale = 1.0;

	if (CroppingParameters.SizingAxis == ECroppingRegionSizingAxis::Horizontal)
	{
		double targetDstRegionWidth = CroppingParameters.Size > 1.0 ? CroppingParameters.Size : CroppingParameters.Size * srcRegionSize.X;
		FVector2D targetDstRegionSize(targetDstRegionWidth, targetDstRegionWidth / dstRegionAspect);

		dstRegionScale = FMath::Min(FMath::Sqrt(targetDstRegionSize.SquaredLength() / dstRegionSize.SquaredLength()), 1.0);
	}
	else if (CroppingParameters.SizingAxis == ECroppingRegionSizingAxis::Vertical)
	{
		double targetDstRegionHeight = CroppingParameters.Size > 1.0 ? CroppingParameters.Size : CroppingParameters.Size * srcRegionSize.Y;
		FVector2D targetDstRegionSize(targetDstRegionHeight * dstRegionAspect, targetDstRegionHeight);

		dstRegionScale = FMath::Min(FMath::Sqrt(targetDstRegionSize.SquaredLength() / dstRegionSize.SquaredLength()), 1.0);
	}
	else
	{
		dstRegionScale = CroppingParameters.Size > 1.0 ? CroppingParameters.Size / srcRegionSize.Length() : CroppingParameters.Size;
	}

	FIntPoint croppingRectSize = (dstRegionSize * dstRegionScale).IntPoint();

	if (croppingRectSize.X < 1 || croppingRectSize.Y < 1)
	{
		UE_LOG(LogGravityCameraFrameGrabber, Warning, TEXT("The resulting cropping area is smaller then one pixel."));

		// No data to copy. Cancel the operation.
		return;
	}

	croppingRect.Max = croppingRect.Min + croppingRectSize;

	check(croppingRect.Max.X <= viewRect.Max.X && croppingRect.Max.Y <= viewRect.Max.Y);

	FRDGTextureRef inputTextureRDG = RegisterExternalTexture(GraphBuilder, inputTextureRHI, TEXT("InputSceneColor"));
	FRDGTextureRef outputTextureRDG = RegisterExternalTexture(GraphBuilder, TextureTargetRHI, TEXT("OutputSceneColor"));

	if (bIsPostProcessingActive)
	{
		FVector4 croppingUV = FVector4(
			croppingRect.Min.X / static_cast<double>(viewRect.Max.X),	// UV0.x
			croppingRect.Min.Y / static_cast<double>(viewRect.Max.Y),	// UV0.y
			croppingRect.Max.X / static_cast<double>(viewRect.Max.X),	// UV1.x
			croppingRect.Max.Y / static_cast<double>(viewRect.Max.Y)	// UV1.y
		);

		FVector4 inputImageSizeAndInvSize = FVector4(
			viewRect.Width(),
			viewRect.Height(),
			1.0 / viewRect.Width(),
			1.0 / viewRect.Height()
		);

		// update material
		check(PostProcessMaterial);
		PostProcessMaterial->SetVectorParameterValue(TEXT("CroppingUV"), croppingUV);
		PostProcessMaterial->SetVectorParameterValue(TEXT("InputImageSizeAndInvSize"), inputImageSizeAndInvSize);

		check(PostProcessTexture);

		if (bGenerateInputMips && !inputTextureDesc.IsMipChain())
		{
			// Create a full mip chain
			int32 numMips = FMath::CeilLogTwo(FMath::Min(inputTextureDesc.Extent.X, inputTextureDesc.Extent.Y)) + 1;

			if (!TextureWithMipsRHI || TextureWithMipsRHI->GetDesc().Extent != inputTextureDesc.Extent || TextureWithMipsRHI->GetDesc().Format != inputTextureDesc.Format)
			{
				FRHITextureCreateDesc textureWithMipsDesc = FRHITextureCreateDesc::Create2D(TEXT("InputSceneColorWithMips"))
					.SetExtent(inputTextureDesc.Extent)
					.SetFormat(inputTextureDesc.Format)
					.SetFlags(inputTextureDesc.Flags | ETextureCreateFlags::UAV)
					.SetNumMips(numMips);

				TextureWithMipsRHI = RHICreateTexture(textureWithMipsDesc);
			}

			// Create a temp texture that will hold mips
			FRDGTextureRef inputTextureWithMipsRDG = RegisterExternalTexture(GraphBuilder, TextureWithMipsRHI, TEXT("InputSceneColorWithMips"));

			AddCopyTexturePass(GraphBuilder, inputTextureRDG, inputTextureWithMipsRDG, FRHICopyTextureInfo());

			FGenerateMipsParams generateMipsParams;
			generateMipsParams.Filter = GetSamplerFilter(PostProcessTexture);
			generateMipsParams.AddressU = PostProcessTexture->SamplerAddressMode;
			generateMipsParams.AddressV = PostProcessTexture->SamplerAddressMode;
			generateMipsParams.AddressW = PostProcessTexture->SamplerAddressMode;

			FGenerateMips::Execute(GraphBuilder, InView.GetFeatureLevel(), inputTextureWithMipsRDG, generateMipsParams);

			// Use the new texture as the input for the post-process material
			PostProcessTexture->UpdateTextureResource(TextureWithMipsRHI);
		}
		else
		{
			PostProcessTexture->UpdateTextureResource(inputTextureRHI);
		}

		FScreenPassRenderTarget outputScreenPassRenderTarget =
			FScreenPassRenderTarget(outputTextureRDG, FIntRect(FIntPoint::ZeroValue, outputTextureDesc.Extent), ERenderTargetLoadAction::ELoad);

		AddDrawCanvasPass(GraphBuilder, RDG_EVENT_NAME("PostProcessGrabbedCameraFrame"), InView, outputScreenPassRenderTarget, [this, ViewSize = outputTextureDesc.Extent, &croppingUV](FCanvas& Canvas)
		{
			FCanvasTileItem tileItem(FVector2D::ZeroVector, PostProcessMaterial->GetRenderProxy(), ViewSize);
			tileItem.BlendMode = ESimpleElementBlendMode::SE_BLEND_Opaque;
			tileItem.UV0 = FVector2D(croppingUV.X, croppingUV.Y);
			tileItem.UV1 = FVector2D(croppingUV.Z, croppingUV.W);

			Canvas.DrawItem(tileItem);
		});
	}
	else
	{
		// Perform a hardware or raster copy. Raster copy will be triggered if the formats of the input and output textures differ.
		AddDrawTexturePass(GraphBuilder, InView, inputTextureRDG, outputTextureRDG, croppingRect.Min, croppingRect.Size(), FIntPoint::ZeroValue, outputTextureDesc.Extent);
	}

	// Grab request processed.
	bIsActive = false;
}

void UCameraFrameGrabberSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	SceneViewExtension = FSceneViewExtensions::NewExtension<FFrameGrabberSceneViewExtension>();

	Super::Initialize(Collection);
}

void UCameraFrameGrabberSubsystem::SetTextureTarget(UTextureRenderTarget2D* InTextureTarget)
{
	// Calling SetTextureTarget() with a nullptr will put the scene extension into a disabled state.
	ENQUEUE_RENDER_COMMAND(CameraFrameGrabberSubsystem_SetTextureTarget)([this, InTextureTarget](FRHICommandList& RHICmdList)
	{
		FTextureRenderTargetResource* textureResource = InTextureTarget->GetRenderTargetResource();

		if (textureResource)
		{
			SceneViewExtension->SetTextureTarget(textureResource->GetTexture2DRHI());
		}
		else
		{
			UE_LOG(LogGravityCameraFrameGrabber, Warning, TEXT("The set texture target is invalid."));
		}
	});

	TextureTarget = InTextureTarget;

	// Readjust the cropping paramters after setting the texture
	SetCroppingParameters(CroppingParameters);
}

void UCameraFrameGrabberSubsystem::SetPostProcessMaterial(UMaterial* InPostProcessMaterial)
{
	// Calling SetPostProcessMaterial() with a nullptr will disable the post-processing pass.
	if (InPostProcessMaterial && InPostProcessMaterial->MaterialDomain != EMaterialDomain::MD_UI)
	{
		UE_LOG(LogGravityCameraFrameGrabber, Error, TEXT("The set post-process material has an invalid material domain. The material domain must be set to 'User Interface'."));

		InPostProcessMaterial = nullptr;
	}

	ENQUEUE_RENDER_COMMAND(CameraFrameGrabberSubsystem_SetPostProcessMaterial)([this, InPostProcessMaterial](FRHICommandList& RHICmdList)
	{
		UMaterialInstanceDynamic* postPocessorMaterialInstance = nullptr;

		if (InPostProcessMaterial)
		{
			postPocessorMaterialInstance = UMaterialInstanceDynamic::Create(InPostProcessMaterial, this);
		}

		SceneViewExtension->SetPostProcessMaterial(postPocessorMaterialInstance);
	});

	PostProcessMaterial = InPostProcessMaterial;
}

void UCameraFrameGrabberSubsystem::SetCroppingParameters(const FRegionOfInterest& InParameters)
{
	FRegionOfInterest adjustedParameters = InParameters;

	if (adjustedParameters.Offset.X < 0 || adjustedParameters.Offset.Y < 0)
	{
		UE_LOG(LogGravityCameraFrameGrabber, Error,
			TEXT("Invalid cropping offset was specified. The offset cannot be less then zero."));

		adjustedParameters.Offset.X = FMath::Max(adjustedParameters.Offset.X, 0);
		adjustedParameters.Offset.Y = FMath::Max(adjustedParameters.Offset.Y, 0);
	}

	if (TextureTarget)
	{ 
		if (adjustedParameters.Size < DOUBLE_KINDA_SMALL_NUMBER)
		{
			UE_LOG(LogGravityCameraFrameGrabber, Error, TEXT("The size of the cropping region must be greater then zero."));

			adjustedParameters.Size = 1.0;
		}
	}

	ENQUEUE_RENDER_COMMAND(CameraFrameGrabberSubsystem_SetCroppingOffset)([this, CroppingParameters = adjustedParameters](FRHICommandList& RHICmdList)
	{
		SceneViewExtension->SetCropping(CroppingParameters);
	});

	CroppingParameters = adjustedParameters;
}

void UCameraFrameGrabberSubsystem::SetGenerateInputMips(bool bShouldGenerateInputMips)
{
	ENQUEUE_RENDER_COMMAND(CameraFrameGrabberSubsystem_SetGenerateInputMipMaps)([this, bShouldGenerateInputMips](FRHICommandList& RHICmdList)
	{
		SceneViewExtension->SetGenerateInputMips(bShouldGenerateInputMips);
	});

	bGenerateInputMips = bShouldGenerateInputMips;
}

void UCameraFrameGrabberSubsystem::GrabFrame()
{
	if (!TextureTarget)
	{
		UE_LOG(LogGravityCameraFrameGrabber, Error, TEXT("Failed to grab frame: The application must set a valid texture target."));

		return;
	}

	if (!CroppingParameters.IsValid())
	{
		UE_LOG(LogGravityCameraFrameGrabber, Error, TEXT("Failed to grab frame: invalid region of interest (ROI) was specified."));

		return;
	}

	ENQUEUE_RENDER_COMMAND(CameraFrameGrabberSubsystem_GrabFrame)([this](FRHICommandList& RHICmdList)
	{
		SceneViewExtension->GrabFrame_RenderThread();
	});
}