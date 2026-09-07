#include "Gameplay/Photo/NPPhotoCapturePenaltyComponent.h"

#include "Engine/World.h"
#include "Gameplay/Character/NPReplicatedStablePhysicsPawn.h"
#include "Gameplay/Interaction/Components/GrabbableComponent.h"
#include "Gameplay/Photo/NPRelicHolderInterface.h"
#include "Gameplay/Photo/NPPhotoStunVisualComponent.h"
#include "Gameplay/Relic/NPBaseRelic.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UI/GameScreen/NPScoreFeedbackWidgetComponent.h"

UNPPhotoCapturePenaltyComponent::UNPPhotoCapturePenaltyComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UNPPhotoCapturePenaltyComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UNPPhotoCapturePenaltyComponent, bPhotoStunActive);
	DOREPLIFETIME(UNPPhotoCapturePenaltyComponent, PhotoStunEndServerTime);
}

void UNPPhotoCapturePenaltyComponent::BeginPlay()
{
	Super::BeginPlay();
	ApplyStunStateLocally();
}

void UNPPhotoCapturePenaltyComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StunTimer);
	}

	bPhotoStunActive = false;
	ApplyStunStateLocally();
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

	const float SafeStunDuration = FMath::Max(0.01f, StunDuration);
	bPhotoStunActive = true;
	PhotoStunEndServerTime = World->GetTimeSeconds() + SafeStunDuration;
	ApplyStunStateLocally();

	World->GetTimerManager().SetTimer(
		StunTimer,
		this,
		&ThisClass::FinishStunPenalty,
		SafeStunDuration,
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

void UNPPhotoCapturePenaltyComponent::OnRep_PhotoStunActive()
{
	ApplyStunStateLocally();
}

void UNPPhotoCapturePenaltyComponent::ApplyStunStateLocally()
{
	ANPReplicatedStablePhysicsPawn* Pawn =
		Cast<ANPReplicatedStablePhysicsPawn>(GetOwner());
	if (!IsValid(Pawn))
	{
		return;
	}

	if (bPhotoStunActive)
	{
		Pawn->StopMovementInput();
	}

	if (UNPPhotoStunVisualComponent* StunVisual =
		Pawn->FindComponentByClass<UNPPhotoStunVisualComponent>())
	{
		StunVisual->SetStunVisualActive(bPhotoStunActive);
	}
}

void UNPPhotoCapturePenaltyComponent::FinishStunPenalty()
{
	ANPReplicatedStablePhysicsPawn* Pawn =
		Cast<ANPReplicatedStablePhysicsPawn>(GetOwner());
	if (!IsValid(Pawn) || !Pawn->HasAuthority())
	{
		return;
	}

	bPhotoStunActive = false;
	PhotoStunEndServerTime = 0.0f;
	ApplyStunStateLocally();
	Pawn->ForceNetUpdate();
}
