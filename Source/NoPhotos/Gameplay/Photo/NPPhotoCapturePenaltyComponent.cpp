#include "Gameplay/Photo/NPPhotoCapturePenaltyComponent.h"

#include "Engine/World.h"
#include "Core/GameplayTag/NPGameplayTags.h"
#include "Gameplay/AbilitySystem/NPAbilitySystemComponent.h"
#include "Gameplay/Character/NPReplicatedStablePhysicsPawn.h"
#include "Gameplay/Interaction/Components/GrabbableComponent.h"
#include "Gameplay/Photo/NPRelicHolderInterface.h"
#include "Gameplay/Photo/NPPhotoStunVisualComponent.h"
#include "Gameplay/Relic/NPBaseRelic.h"
#include "Net/UnrealNetwork.h"
#include "NoPhotos.h"
#include "Gameplay/Photo/NPPhotoLog.h"
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
	UE_LOG(
		LogNPPhoto,
		Warning,
		TEXT("[PhotoStun][Apply] Enter Owner=%s Pawn=%s Role=%s Relic=%s Returned=%s World=%s"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(Pawn),
		Pawn ? *UEnum::GetValueAsString(Pawn->GetLocalRole()) : TEXT("None"),
		*GetNameSafe(EvidenceRelic),
		IsValid(EvidenceRelic) && EvidenceRelic->IsReturned()
			? TEXT("true")
			: TEXT("false"),
		*GetNameSafe(World));
	if (!IsValid(Pawn) || !Pawn->HasAuthority() || !IsValid(EvidenceRelic)
		|| !World || EvidenceRelic->IsReturned())
	{
		UE_LOG(
			LogNPPhoto,
			Error,
			TEXT("[PhotoStun][Apply] Rejected: invalid context. PawnValid=%s Authority=%s RelicValid=%s WorldValid=%s Returned=%s"),
			IsValid(Pawn) ? TEXT("true") : TEXT("false"),
			IsValid(Pawn) && Pawn->HasAuthority() ? TEXT("true") : TEXT("false"),
			IsValid(EvidenceRelic) ? TEXT("true") : TEXT("false"),
			World ? TEXT("true") : TEXT("false"),
			IsValid(EvidenceRelic) && EvidenceRelic->IsReturned()
				? TEXT("true")
				: TEXT("false"));
		return false;
	}

	AActor* HeldRelic = INPRelicHolderInterface::Execute_GetHeldRelic(Pawn);
	if (HeldRelic != EvidenceRelic)
	{
		UE_LOG(
			LogNPPhoto,
			Error,
			TEXT("[PhotoStun][Apply] Rejected: held relic mismatch. Pawn=%s Held=%s Evidence=%s"),
			*GetNameSafe(Pawn),
			*GetNameSafe(HeldRelic),
			*GetNameSafe(EvidenceRelic));
		return false;
	}

	UGrabbableComponent* Grabbable =
		EvidenceRelic->FindComponentByClass<UGrabbableComponent>();
	if (!IsValid(Grabbable) || !Grabbable->IsGrabbed())
	{
		UE_LOG(
			LogNPPhoto,
			Error,
			TEXT("[PhotoStun][Apply] Rejected: invalid grab state. Relic=%s Grabbable=%s IsGrabbed=%s"),
			*GetNameSafe(EvidenceRelic),
			*GetNameSafe(Grabbable),
			IsValid(Grabbable) && Grabbable->IsGrabbed()
				? TEXT("true")
				: TEXT("false"));
		return false;
	}

	// 유물 자체를 떨어뜨리는 규칙이므로 함께 잡은 다른 플레이어의 Grab도 해제합니다.
	Grabbable->ForceReleaseAllGrabs();

	const float SafeStunDuration = FMath::Max(0.01f, StunDuration);
	bPhotoStunActive = true;
	PhotoStunEndServerTime = World->GetTimeSeconds() + SafeStunDuration;
	UE_LOG(
		LogNPPhoto,
		Warning,
		TEXT("[PhotoStun][Apply] Activated. Pawn=%s Duration=%.2f EndTime=%.3f"),
		*GetNameSafe(Pawn),
		SafeStunDuration,
		PhotoStunEndServerTime);
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
	const APawn* Pawn = Cast<APawn>(GetOwner());
	UE_LOG(
		LogNPPhoto,
		Warning,
		TEXT("[PhotoStun][Rep] Pawn=%s Active=%s EndTime=%.3f LocalRole=%s LocallyControlled=%s"),
		*GetNameSafe(Pawn),
		bPhotoStunActive ? TEXT("true") : TEXT("false"),
		PhotoStunEndServerTime,
		Pawn ? *UEnum::GetValueAsString(Pawn->GetLocalRole()) : TEXT("None"),
		Pawn && Pawn->IsLocallyControlled() ? TEXT("true") : TEXT("false"));
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

	if (UNPAbilitySystemComponent* AbilitySystem =
		Cast<UNPAbilitySystemComponent>(Pawn->GetAbilitySystemComponent()))
	{
		AbilitySystem->SetLooseGameplayTagCount(
			NPGameplayTags::State_CrowdControl_Stunned,
			bPhotoStunActive ? 1 : 0);
		if (bPhotoStunActive)
		{
			AbilitySystem->CancelRelicAimAbility();
			AbilitySystem->CancelPhotoAimAbility();
		}
	}

	UNPPhotoStunVisualComponent* StunVisual =
		Pawn->FindComponentByClass<UNPPhotoStunVisualComponent>();
	UE_LOG(
		LogNPPhoto,
		Warning,
		TEXT("[PhotoStun][VisualLookup] Pawn=%s Active=%s Visual=%s"),
		*GetNameSafe(Pawn),
		bPhotoStunActive ? TEXT("true") : TEXT("false"),
		*GetNameSafe(StunVisual));
	if (StunVisual)
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
