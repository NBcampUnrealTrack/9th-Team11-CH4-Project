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
	CreateMissionBonusWidgetComponent();
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
	if (IsValid(MissionBonusWidgetComponent))
	{
		MissionBonusWidgetComponent->DestroyComponent();
		MissionBonusWidgetComponent = nullptr;
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

void UNPScoreFeedbackWidgetComponent::ShowRelicReturnFeedback(
	const int32 ReturnScore,
	const int32 MissionBonusScore,
	const float DurationSeconds)
{
	const AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority()
		|| ReturnScore < 0 || MissionBonusScore <= 0)
	{
		return;
	}

	MulticastShowRelicReturnFeedback(
		ReturnScore,
		FMath::Max(0, MissionBonusScore),
		FMath::Max(0.01f, DurationSeconds));
}

void UNPScoreFeedbackWidgetComponent::MulticastShowRelicReturnFeedback_Implementation(
	const int32 ReturnScore,
	const int32 MissionBonusScore,
	const float DurationSeconds)
{
	ShowRelicReturnFeedbackLocally(
		ReturnScore,
		MissionBonusScore,
		DurationSeconds);
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

	UNPScoreFeedbackWidget* FeedbackWidget = ResolveFeedbackWidget();
	if (!IsValid(FeedbackWidget))
	{
		return;
	}

	FeedbackWidget->SetScoreFeedback(Amount, FeedbackType);
	if (IsValid(MissionBonusWidgetComponent))
	{
		MissionBonusWidgetComponent->SetVisibility(false, true);
	}
	BeginDisplayingFeedback(DurationSeconds);

	UE_LOG(
		LogNPPhoto,
		Log,
		TEXT("[ScoreFeedbackUI] Feedback shown. Owner=%s Amount=%d Type=%d Duration=%.2f"),
		*GetNameSafe(GetOwner()),
		Amount,
		static_cast<int32>(FeedbackType),
		DurationSeconds);
}

void UNPScoreFeedbackWidgetComponent::ShowRelicReturnFeedbackLocally(
	const int32 ReturnScore,
	const int32 MissionBonusScore,
	const float DurationSeconds)
{
	if (ReturnScore < 0 || MissionBonusScore <= 0
		|| GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	UNPScoreFeedbackWidget* FeedbackWidget = ResolveFeedbackWidget();
	UNPScoreFeedbackWidget* MissionBonusWidget = ResolveMissionBonusWidget();
	if (!IsValid(FeedbackWidget) || !IsValid(MissionBonusWidget))
	{
		return;
	}

	FeedbackWidget->SetScoreFeedback(
		ReturnScore,
		ENPScoreFeedbackType::RelicReturnReward);
	MissionBonusWidget->SetScoreFeedback(
		MissionBonusScore,
		ENPScoreFeedbackType::PersonalMissionBonus);
	MissionBonusWidgetComponent->SetVisibility(true, true);
	BeginDisplayingFeedback(DurationSeconds);
}

void UNPScoreFeedbackWidgetComponent::CreateMissionBonusWidgetComponent()
{
	if (IsValid(MissionBonusWidgetComponent) || !IsValid(GetOwner()))
	{
		return;
	}

	MissionBonusWidgetComponent = NewObject<UWidgetComponent>(
		GetOwner(),
		TEXT("MissionBonusFeedbackWidget"));
	MissionBonusWidgetComponent->SetupAttachment(this);
	MissionBonusWidgetComponent->SetWidgetClass(GetWidgetClass());
	MissionBonusWidgetComponent->SetWidgetSpace(GetWidgetSpace());
	MissionBonusWidgetComponent->SetDrawSize(GetDrawSize());
	MissionBonusWidgetComponent->SetPivot(GetPivot());
	MissionBonusWidgetComponent->SetTwoSided(GetTwoSided());
	MissionBonusWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MissionBonusWidgetComponent->SetRelativeLocation(MissionBonusRelativeLocation);
	MissionBonusWidgetComponent->RegisterComponent();
	MissionBonusWidgetComponent->SetVisibility(false, true);
}

UNPScoreFeedbackWidget* UNPScoreFeedbackWidgetComponent::ResolveMissionBonusWidget()
{
	CreateMissionBonusWidgetComponent();
	if (!IsValid(MissionBonusWidgetComponent))
	{
		return nullptr;
	}

	MissionBonusWidgetComponent->InitWidget();
	return Cast<UNPScoreFeedbackWidget>(
		MissionBonusWidgetComponent->GetUserWidgetObject());
}

UNPScoreFeedbackWidget* UNPScoreFeedbackWidgetComponent::ResolveFeedbackWidget()
{
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
	}

	return FeedbackWidget;
}

void UNPScoreFeedbackWidgetComponent::BeginDisplayingFeedback(
	const float DurationSeconds)
{
	SetVisibility(true, false);
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
	if (IsValid(MissionBonusWidgetComponent))
	{
		MissionBonusWidgetComponent->SetVisibility(false, true);
	}
	SetVisibility(false, false);
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
