#include "Gameplay/Photo/NPPhotoCapturePenaltyComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Gameplay/Character/NPReplicatedStablePhysicsPawn.h"
#include "Gameplay/Character/Component/NPStablePhysicsMovementComponent.h"
#include "Gameplay/Interaction/Components/GrabbableComponent.h"
#include "Gameplay/Photo/NPRelicHolderInterface.h"
#include "Gameplay/Relic/NPBaseRelic.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UI/GameScreen/NPScoreFeedbackWidgetComponent.h"

namespace
{
const FName PhotoCapturedSpeedSource(TEXT("PhotoCaptured"));
}

UNPPhotoCapturePenaltyComponent::UNPPhotoCapturePenaltyComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UNPPhotoCapturePenaltyComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UNPPhotoCapturePenaltyComponent, bPhotoSlowActive);
	DOREPLIFETIME(UNPPhotoCapturePenaltyComponent, PhotoSlowEndServerTime);
}

void UNPPhotoCapturePenaltyComponent::BeginPlay()
{
	Super::BeginPlay();
	ApplySlowStateLocally();
}

void UNPPhotoCapturePenaltyComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SlowTimer);
	}

	bPhotoSlowActive = false;
	ApplySlowStateLocally();
	Super::EndPlay(EndPlayReason);
}

bool UNPPhotoCapturePenaltyComponent::ApplyCapturedWithRelicPenalty(
	ANPBaseRelic* EvidenceRelic,
	const int32 AppliedPhotoPenalty)
{
	ANPReplicatedStablePhysicsPawn* Pawn =
		Cast<ANPReplicatedStablePhysicsPawn>(GetOwner());
	UWorld* World = GetWorld();
	if (!IsValid(Pawn) || !Pawn->HasAuthority() || !IsValid(EvidenceRelic)
		|| !World || EvidenceRelic->IsReturned())
	{
		return false;
	}

	AActor* HeldRelic = INPRelicHolderInterface::Execute_GetHeldRelic(Pawn);
	if (HeldRelic != EvidenceRelic)
	{
		return false;
	}

	UGrabbableComponent* Grabbable =
		EvidenceRelic->FindComponentByClass<UGrabbableComponent>();
	if (!IsValid(Grabbable) || !Grabbable->IsGrabbed())
	{
		return false;
	}

	// 유물 자체를 떨어뜨리는 규칙이므로 함께 잡은 다른 플레이어의 Grab도 해제합니다.
	Grabbable->ForceReleaseAllGrabs();

	const float SafeSlowDuration = FMath::Max(0.01f, SlowDuration);
	bPhotoSlowActive = true;
	PhotoSlowEndServerTime = World->GetTimeSeconds() + SafeSlowDuration;
	ApplySlowStateLocally();

	World->GetTimerManager().SetTimer(
		SlowTimer,
		this,
		&ThisClass::FinishSlowPenalty,
		SafeSlowDuration,
		false);
	if (AppliedPhotoPenalty > 0)
	{
		if (UNPScoreFeedbackWidgetComponent* ScoreFeedback =
			Pawn->FindComponentByClass<UNPScoreFeedbackWidgetComponent>())
		{
			ScoreFeedback->ShowScoreFeedback(
				AppliedPhotoPenalty,
				ENPScoreFeedbackType::PhotoPenalty,
				PriceReductionMessageDuration);
		}
	}
	Pawn->ForceNetUpdate();
	return true;
}

void UNPPhotoCapturePenaltyComponent::OnRep_PhotoSlowActive()
{
	ApplySlowStateLocally();
}

void UNPPhotoCapturePenaltyComponent::ApplySlowStateLocally()
{
	ANPReplicatedStablePhysicsPawn* Pawn =
		Cast<ANPReplicatedStablePhysicsPawn>(GetOwner());
	if (!IsValid(Pawn))
	{
		return;
	}

	if (UNPStablePhysicsMovementComponent* Movement =
		Pawn->GetStablePhysicsMovementComponent())
	{
		if (bPhotoSlowActive)
		{
			Movement->SetMoveSpeedMultiplier(
				PhotoCapturedSpeedSource,
				FMath::Clamp(MoveSpeedMultiplier, 0.0f, 1.0f));
		}
		else
		{
			Movement->ClearMoveSpeedMultiplier(PhotoCapturedSpeedSource);
		}
	}

	if (USkeletalMeshComponent* PhysicsMesh =
		Pawn->FindComponentByClass<USkeletalMeshComponent>())
	{
		PhysicsMesh->SetOverlayMaterial(
			bPhotoSlowActive ? SlowOverlayMaterial.Get() : nullptr);
	}
}

void UNPPhotoCapturePenaltyComponent::FinishSlowPenalty()
{
	ANPReplicatedStablePhysicsPawn* Pawn =
		Cast<ANPReplicatedStablePhysicsPawn>(GetOwner());
	if (!IsValid(Pawn) || !Pawn->HasAuthority())
	{
		return;
	}

	bPhotoSlowActive = false;
	PhotoSlowEndServerTime = 0.0f;
	ApplySlowStateLocally();
	Pawn->ForceNetUpdate();
}
