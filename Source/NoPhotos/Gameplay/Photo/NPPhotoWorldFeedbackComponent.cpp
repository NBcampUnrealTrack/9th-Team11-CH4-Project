#include "Gameplay/Photo/NPPhotoWorldFeedbackComponent.h"

#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

UNPPhotoWorldFeedbackComponent::UNPPhotoWorldFeedbackComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetComponentTickEnabled(false);
	SetHiddenInGame(true);
	SetVisibility(false);

	// Decal은 로컬 X축 방향으로 투영되므로 기본 상태에서 바닥을 향하게 합니다.
	SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
	SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	DecalSize = FVector(MaximumDecalSize.X, 0.0f, 0.0f);
}

void UNPPhotoWorldFeedbackComponent::BeginPlay()
{
	Super::BeginPlay();
	StopFeedback();
}

void UNPPhotoWorldFeedbackComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsPlaying)
	{
		return;
	}

	ElapsedTime += FMath::Max(DeltaTime, 0.0f);
	const float SafeGrowDuration = FMath::Max(GrowDuration, 0.0f);
	const float SafeHoldDuration = FMath::Max(HoldDuration, 0.0f);
	const float SafeShrinkDuration = FMath::Max(ShrinkDuration, 0.0f);
	const float HoldEndTime = SafeGrowDuration + SafeHoldDuration;
	const float TotalDuration = HoldEndTime + SafeShrinkDuration;

	if (SafeGrowDuration > 0.0f && ElapsedTime < SafeGrowDuration)
	{
		const float Alpha = FMath::Clamp(ElapsedTime / SafeGrowDuration, 0.0f, 1.0f);
		const float EasedAlpha = FMath::InterpEaseOut(0.0f, 1.0f, Alpha, 3.0f);
		ApplyAnimationState(EasedAlpha, EasedAlpha);
		return;
	}

	if (ElapsedTime < HoldEndTime)
	{
		ApplyAnimationState(1.0f, 1.0f);
		return;
	}

	if (SafeShrinkDuration > 0.0f && ElapsedTime < TotalDuration)
	{
		const float Alpha = FMath::Clamp(
			(ElapsedTime - HoldEndTime) / SafeShrinkDuration,
			0.0f,
			1.0f);
		const float RemainingAlpha = FMath::InterpEaseIn(1.0f, 0.0f, Alpha, 2.0f);
		ApplyAnimationState(RemainingAlpha, RemainingAlpha);
		return;
	}

	StopFeedback();
}

void UNPPhotoWorldFeedbackComponent::PlayPhotographerEffect()
{
	PlayFeedback(PhotographerMaterial);
}

void UNPPhotoWorldFeedbackComponent::PlayPhotographedEffect()
{
	PlayFeedback(PhotographedMaterial);
	PlayLocalPhotographedCameraShake();
}

void UNPPhotoWorldFeedbackComponent::StopFeedback()
{
	bIsPlaying = false;
	ElapsedTime = 0.0f;
	ApplyAnimationState(0.0f, 0.0f);
	SetVisibility(false);
	SetHiddenInGame(true);
	SetComponentTickEnabled(false);
}

void UNPPhotoWorldFeedbackComponent::PlayFeedback(
	UMaterialInterface* FeedbackMaterial)
{
	if (!IsValid(FeedbackMaterial))
	{
		StopFeedback();
		return;
	}

	SetDecalMaterial(FeedbackMaterial);
	DynamicMaterial = CreateDynamicMaterialInstance();
	if (!IsValid(DynamicMaterial))
	{
		StopFeedback();
		return;
	}

	ElapsedTime = 0.0f;
	bIsPlaying = true;
	SetHiddenInGame(false);
	SetVisibility(true);
	SetComponentTickEnabled(true);
	ApplyAnimationState(0.0f, 0.0f);
}

void UNPPhotoWorldFeedbackComponent::PlayLocalPhotographedCameraShake()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!IsValid(OwnerPawn)
		|| !OwnerPawn->IsLocallyControlled()
		|| !PhotographedCameraShake)
	{
		return;
	}

	APlayerController* LocalPlayerController =
		Cast<APlayerController>(OwnerPawn->GetController());
	if (!IsValid(LocalPlayerController)
		|| !IsValid(LocalPlayerController->PlayerCameraManager))
	{
		return;
	}

	LocalPlayerController->PlayerCameraManager->StartCameraShake(
		PhotographedCameraShake,
		FMath::Max(PhotographedCameraShakeScale, 0.0f));
}

void UNPPhotoWorldFeedbackComponent::ApplyAnimationState(
	float SizeAlpha,
	float OpacityAlpha)
{
	const float SafeSizeAlpha = FMath::Clamp(SizeAlpha, 0.0f, 1.0f);
	DecalSize = FVector(
		FMath::Max(MaximumDecalSize.X, 0.0f),
		FMath::Max(MaximumDecalSize.Y, 0.0f) * SafeSizeAlpha,
		FMath::Max(MaximumDecalSize.Z, 0.0f) * SafeSizeAlpha);
	MarkRenderStateDirty();

	if (IsValid(DynamicMaterial) && !OpacityParameterName.IsNone())
	{
		DynamicMaterial->SetScalarParameterValue(
			OpacityParameterName,
			FMath::Clamp(OpacityAlpha, 0.0f, 1.0f)
				* FMath::Clamp(MaximumOpacity, 0.0f, 1.0f));
	}
}
