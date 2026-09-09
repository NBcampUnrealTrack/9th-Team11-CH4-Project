#include "Gameplay/Photo/NPPhotoCapturePenaltyComponent.h"

#include "Core/GameplayTag/NPGameplayTags.h"
#include "Gameplay/AbilitySystem/NPAbilitySystemComponent.h"
#include "Gameplay/AbilitySystem/Effects/NPPhotoStunGameplayEffect.h"
#include "Gameplay/Character/NPReplicatedStablePhysicsPawn.h"
#include "Gameplay/Interaction/Components/GrabbableComponent.h"
#include "Gameplay/Photo/NPRelicHolderInterface.h"
#include "Gameplay/Relic/NPBaseRelic.h"
#include "NoPhotos.h"
#include "Gameplay/Photo/NPPhotoLog.h"
#include "UI/GameScreen/NPScoreFeedbackWidgetComponent.h"

UNPPhotoCapturePenaltyComponent::UNPPhotoCapturePenaltyComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UNPPhotoCapturePenaltyComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UNPAbilitySystemComponent* AbilitySystem = ResolveAbilitySystem())
	{
		StunTagChangedHandle = AbilitySystem->RegisterGameplayTagEvent(
			NPGameplayTags::State_CrowdControl_Stunned,
			EGameplayTagEventType::NewOrRemoved).AddUObject(
				this,
				&ThisClass::HandleStunTagChanged);
	}
	ApplyStunStateLocally(IsPhotoStunActive());
}

void UNPPhotoCapturePenaltyComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (UNPAbilitySystemComponent* AbilitySystem = ResolveAbilitySystem();
		AbilitySystem && StunTagChangedHandle.IsValid())
	{
		AbilitySystem->RegisterGameplayTagEvent(
			NPGameplayTags::State_CrowdControl_Stunned,
			EGameplayTagEventType::NewOrRemoved).Remove(StunTagChangedHandle);
	}
	StunTagChangedHandle.Reset();
	ApplyStunStateLocally(false);
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

	UNPAbilitySystemComponent* AbilitySystem = ResolveAbilitySystem();
	if (!AbilitySystem)
	{
		UE_LOG(
			LogNPPhoto,
			Error,
			TEXT("[PhotoStun][Apply] Rejected: ASC is missing. Pawn=%s"),
			*GetNameSafe(Pawn));
		return false;
	}

	const float SafeStunDuration = FMath::Max(0.01f, StunDuration);
	FGameplayEffectContextHandle EffectContext = AbilitySystem->MakeEffectContext();
	EffectContext.AddSourceObject(this);
	FGameplayEffectSpecHandle EffectSpec = AbilitySystem->MakeOutgoingSpec(
		UNPPhotoStunGameplayEffect::StaticClass(),
		1.0f,
		EffectContext);
	if (!EffectSpec.IsValid())
	{
		UE_LOG(
			LogNPPhoto,
			Error,
			TEXT("[PhotoStun][Apply] Failed to create GameplayEffect spec. Pawn=%s"),
			*GetNameSafe(Pawn));
		return false;
	}
	EffectSpec.Data->SetDuration(SafeStunDuration, true);
	const FActiveGameplayEffectHandle EffectHandle =
		AbilitySystem->ApplyGameplayEffectSpecToSelf(*EffectSpec.Data.Get());
	if (!EffectHandle.IsValid())
	{
		UE_LOG(
			LogNPPhoto,
			Error,
			TEXT("[PhotoStun][Apply] GameplayEffect application failed. Pawn=%s"),
			*GetNameSafe(Pawn));
		return false;
	}

	// 유물 자체를 떨어뜨리는 규칙이므로 함께 잡은 다른 플레이어의 Grab도 해제합니다.
	Grabbable->ForceReleaseAllGrabs();

	UE_LOG(
		LogNPPhoto,
		Warning,
		TEXT("[PhotoStun][Apply] GameplayEffect activated. Pawn=%s Duration=%.2f Handle=%s"),
		*GetNameSafe(Pawn),
		SafeStunDuration,
		*EffectHandle.ToString());
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
	return true;
}

bool UNPPhotoCapturePenaltyComponent::IsPhotoStunActive() const
{
	const UNPAbilitySystemComponent* AbilitySystem = ResolveAbilitySystem();
	return AbilitySystem && AbilitySystem->HasMatchingGameplayTag(
		NPGameplayTags::State_CrowdControl_Stunned);
}

void UNPPhotoCapturePenaltyComponent::HandleStunTagChanged(
	const FGameplayTag,
	const int32 NewCount)
{
	ApplyStunStateLocally(NewCount > 0);
}

void UNPPhotoCapturePenaltyComponent::ApplyStunStateLocally(const bool bStunned)
{
	ANPReplicatedStablePhysicsPawn* Pawn =
		Cast<ANPReplicatedStablePhysicsPawn>(GetOwner());
	if (!IsValid(Pawn))
	{
		return;
	}

	if (bStunned)
	{
		Pawn->StopMovementInput();
		Pawn->CancelGrabForPhotoStun();
	}

	if (bStunned)
	{
		if (UNPAbilitySystemComponent* AbilitySystem = ResolveAbilitySystem())
		{
			AbilitySystem->CancelRelicAimAbility();
			AbilitySystem->CancelPhotoAimAbility();
		}
	}
}

UNPAbilitySystemComponent*
UNPPhotoCapturePenaltyComponent::ResolveAbilitySystem() const
{
	const ANPReplicatedStablePhysicsPawn* Pawn =
		Cast<ANPReplicatedStablePhysicsPawn>(GetOwner());
	return Pawn
		? Cast<UNPAbilitySystemComponent>(Pawn->GetAbilitySystemComponent())
		: nullptr;
}
