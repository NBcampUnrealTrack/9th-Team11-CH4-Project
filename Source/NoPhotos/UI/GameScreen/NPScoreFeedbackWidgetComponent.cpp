#include "UI/GameScreen/NPScoreFeedbackWidgetComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "Gameplay/Photo/NPPhotoLog.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "TimerManager.h"

UNPScoreFeedbackWidgetComponent::UNPScoreFeedbackWidgetComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);

	SetWidgetSpace(EWidgetSpace::World);
	SetDrawSize(FVector2D(320.0f, 80.0f));
	SetPivot(FVector2D(0.5f, 0.5f));
	SetTwoSided(true);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetOwnerNoSee(false);
	SetOnlyOwnerSee(false);
}

void UNPScoreFeedbackWidgetComponent::BeginPlay()
{
	Super::BeginPlay();
	SetVisibility(false, true);
	SetComponentTickEnabled(false);
}

void UNPScoreFeedbackWidgetComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HideTimer);
	}

	Super::EndPlay(EndPlayReason);
}

void UNPScoreFeedbackWidgetComponent::ShowScoreFeedback(
	const int32 Amount,
	const ENPScoreFeedbackType FeedbackType,
	const float DurationSeconds)
{
	const AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority() || Amount <= 0)
	{
		return;
	}

	MulticastShowScoreFeedback(
		Amount,
		FeedbackType,
		FMath::Max(0.01f, DurationSeconds));
}

void UNPScoreFeedbackWidgetComponent::MulticastShowScoreFeedback_Implementation(
	const int32 Amount,
	const ENPScoreFeedbackType FeedbackType,
	const float DurationSeconds)
{
	ShowScoreFeedbackLocally(Amount, FeedbackType, DurationSeconds);
}

void UNPScoreFeedbackWidgetComponent::ShowScoreFeedbackLocally(
	const int32 Amount,
	const ENPScoreFeedbackType FeedbackType,
	const float DurationSeconds)
{
	if (Amount <= 0 || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	InitWidget();
	UNPScoreFeedbackWidget* FeedbackWidget =
		Cast<UNPScoreFeedbackWidget>(GetUserWidgetObject());
	if (!IsValid(FeedbackWidget))
	{
		UE_LOG(
			LogNPPhoto,
			Warning,
			TEXT("[ScoreFeedbackUI] Invalid WidgetClass. Owner=%s Component=%s Widget=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(this),
			*GetNameSafe(GetUserWidgetObject()));
		return;
	}

	FeedbackWidget->SetScoreFeedback(Amount, FeedbackType);
	SetVisibility(true, true);
	SetComponentTickEnabled(true);
	UpdateFacingCamera();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			HideTimer,
			this,
			&ThisClass::HideFeedback,
			FMath::Max(0.01f, DurationSeconds),
			false);
	}

	UE_LOG(
		LogNPPhoto,
		Log,
		TEXT("[ScoreFeedbackUI] Feedback shown. Owner=%s Amount=%d Type=%d Duration=%.2f"),
		*GetNameSafe(GetOwner()),
		Amount,
		static_cast<int32>(FeedbackType),
		DurationSeconds);
}

void UNPScoreFeedbackWidgetComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateFacingCamera();
}

void UNPScoreFeedbackWidgetComponent::HideFeedback()
{
	SetVisibility(false, true);
	SetComponentTickEnabled(false);
}

void UNPScoreFeedbackWidgetComponent::UpdateFacingCamera()
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
