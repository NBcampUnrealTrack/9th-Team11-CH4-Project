#include "UI/GameScreen/NPPhotoPenaltyWidgetComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "Gameplay/Photo/NPPhotoLog.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "TimerManager.h"
#include "UI/GameScreen/NPPhotoPenaltyWidget.h"

UNPPhotoPenaltyWidgetComponent::UNPPhotoPenaltyWidgetComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	SetWidgetSpace(EWidgetSpace::World);
	SetDrawSize(FVector2D(320.0f, 80.0f));
	SetPivot(FVector2D(0.5f, 0.5f));
	SetTwoSided(true);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetOwnerNoSee(false);
	SetOnlyOwnerSee(false);
}

void UNPPhotoPenaltyWidgetComponent::BeginPlay()
{
	Super::BeginPlay();
	SetVisibility(false, true);
	SetComponentTickEnabled(false);
}

void UNPPhotoPenaltyWidgetComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HideTimer);
	}

	Super::EndPlay(EndPlayReason);
}

void UNPPhotoPenaltyWidgetComponent::ShowPenalty(
	const int32 AppliedPhotoPenalty,
	const float DurationSeconds)
{
	if (AppliedPhotoPenalty <= 0 || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	InitWidget();
	UNPPhotoPenaltyWidget* PenaltyWidget =
		Cast<UNPPhotoPenaltyWidget>(GetUserWidgetObject());
	if (!IsValid(PenaltyWidget))
	{
		UE_LOG(
			LogNPPhoto,
			Warning,
			TEXT("[PhotoPenaltyUI] Invalid WidgetClass. Owner=%s Component=%s Widget=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(this),
			*GetNameSafe(GetUserWidgetObject()));
		return;
	}

	PenaltyWidget->SetPenaltyAmount(AppliedPhotoPenalty);
	SetVisibility(true, true);
	SetComponentTickEnabled(true);
	UpdateFacingCamera();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			HideTimer,
			this,
			&ThisClass::HidePenalty,
			FMath::Max(0.01f, DurationSeconds),
			false);
	}

	UE_LOG(
		LogNPPhoto,
		Log,
		TEXT("[PhotoPenaltyUI] Penalty shown. Owner=%s Amount=%d Duration=%.2f"),
		*GetNameSafe(GetOwner()),
		AppliedPhotoPenalty,
		DurationSeconds);
}

void UNPPhotoPenaltyWidgetComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateFacingCamera();
}

void UNPPhotoPenaltyWidgetComponent::HidePenalty()
{
	SetVisibility(false, true);
	SetComponentTickEnabled(false);
}

void UNPPhotoPenaltyWidgetComponent::UpdateFacingCamera()
{
	APlayerCameraManager* CameraManager =
		UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!IsValid(CameraManager))
	{
		return;
	}

	SetWorldRotation(UKismetMathLibrary::FindLookAtRotation(
		GetComponentLocation(),
		CameraManager->GetCameraLocation()));
}
