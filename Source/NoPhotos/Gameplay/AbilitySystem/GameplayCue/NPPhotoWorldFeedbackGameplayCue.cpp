#include "Gameplay/AbilitySystem/GameplayCue/NPPhotoWorldFeedbackGameplayCue.h"

#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/DecalComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

ANPPhotoWorldFeedbackGameplayCue::ANPPhotoWorldFeedbackGameplayCue()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	SceneRoot->SetAbsolute(false, true, false);

	FeedbackDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("FeedbackDecal"));
	FeedbackDecal->SetupAttachment(SceneRoot);
	FeedbackDecal->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
	FeedbackDecal->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	FeedbackDecal->DecalSize = FVector(MaximumDecalSize.X, 0.0f, 0.0f);
	FeedbackDecal->SetVisibility(false);
	FeedbackDecal->SetHiddenInGame(true);
}

bool ANPPhotoWorldFeedbackGameplayCue::OnExecute_Implementation(
	AActor* Target, const FGameplayCueParameters& Parameters)
{
	Super::OnExecute_Implementation(Target, Parameters);
	ResetFeedback();
	SceneRoot->SetWorldRotation(FRotator::ZeroRotator);

	if (GetNetMode() == NM_DedicatedServer)
	{
		GameplayCueFinishedCallback();
		return true;
	}

	UMaterialInterface* Material = IsValid(FeedbackMaterial)
		? FeedbackMaterial.Get()
		: FeedbackDecal->GetDecalMaterial();
	if (!IsValid(Material))
	{
		GameplayCueFinishedCallback();
		return true;
	}

	FeedbackMaterial = Material;
	FeedbackDecal->SetDecalMaterial(Material);
	DynamicMaterial = FeedbackDecal->CreateDynamicMaterialInstance();
	if (!IsValid(DynamicMaterial))
	{
		GameplayCueFinishedCallback();
		return true;
	}

	ElapsedTime = 0.0f;
	bIsPlaying = true;
	SetActorHiddenInGame(false);
	FeedbackDecal->SetHiddenInGame(false);
	FeedbackDecal->SetVisibility(true);
	SetActorTickEnabled(true);
	ApplyAnimationState(0.0f, 0.0f);
	PlayLocalCameraShake(Target);
	return true;
}

bool ANPPhotoWorldFeedbackGameplayCue::Recycle()
{
	ResetFeedback();
	return Super::Recycle();
}

void ANPPhotoWorldFeedbackGameplayCue::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bIsPlaying)
	{
		return;
	}

	ElapsedTime += FMath::Max(DeltaSeconds, 0.0f);
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

	ResetFeedback();
	GameplayCueFinishedCallback();
}

void ANPPhotoWorldFeedbackGameplayCue::ResetFeedback()
{
	bIsPlaying = false;
	ElapsedTime = 0.0f;
	SetActorTickEnabled(false);
	FeedbackDecal->SetVisibility(false);
	FeedbackDecal->SetHiddenInGame(true);
	DynamicMaterial = nullptr;
}

void ANPPhotoWorldFeedbackGameplayCue::PlayLocalCameraShake(AActor* Target) const
{
	APawn* TargetPawn = Cast<APawn>(Target);
	if (!IsValid(TargetPawn) || !TargetPawn->IsLocallyControlled() || !LocalCameraShake)
	{
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(TargetPawn->GetController());
	if (!IsValid(PlayerController) || !IsValid(PlayerController->PlayerCameraManager))
	{
		return;
	}

	PlayerController->PlayerCameraManager->StartCameraShake(
		LocalCameraShake,
		FMath::Max(LocalCameraShakeScale, 0.0f));
}

void ANPPhotoWorldFeedbackGameplayCue::ApplyAnimationState(
	float SizeAlpha,
	float OpacityAlpha)
{
	const float SafeSizeAlpha = FMath::Clamp(SizeAlpha, 0.0f, 1.0f);
	FeedbackDecal->DecalSize = FVector(
		FMath::Max(MaximumDecalSize.X, 0.0f),
		FMath::Max(MaximumDecalSize.Y, 0.0f) * SafeSizeAlpha,
		FMath::Max(MaximumDecalSize.Z, 0.0f) * SafeSizeAlpha);
	FeedbackDecal->MarkRenderStateDirty();

	if (IsValid(DynamicMaterial) && !OpacityParameterName.IsNone())
	{
		DynamicMaterial->SetScalarParameterValue(
			OpacityParameterName,
			FMath::Clamp(OpacityAlpha, 0.0f, 1.0f)
				* FMath::Clamp(MaximumOpacity, 0.0f, 1.0f));
	}
}
