#include "Gameplay/Relic/Abilities/NPRelicAimAbility.h"

#include "AbilitySystemComponent.h"
#include "Core/GameplayTag/NPGameplayTags.h"
#include "Gameplay/Character/NPStablePhysicsPawn.h"
#include "Gameplay/Interaction/Components/GrabbableComponent.h"
#include "Gameplay/Relic/Components/NPAimableRelicComponent.h"
#include "Gameplay/Relic/Components/NPThrowableRelicComponent.h"

UNPRelicAimAbility::UNPRelicAimAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer Tags;
	Tags.AddTag(NPGameplayTags::Input_Relic_Aim);
	Tags.AddTag(NPGameplayTags::Ability_Relic);
	Tags.AddTag(NPGameplayTags::Ability_Relic_Aim);
	SetAssetTags(Tags);
	ActivationOwnedTags.AddTag(NPGameplayTags::State_Relic_Aiming);
	ActivationBlockedTags.AddTag(
		NPGameplayTags::State_CrowdControl_Stunned);
	ActivationBlockedTags.AddTag(NPGameplayTags::State_Photo_Aiming);
}

bool UNPRelicAimAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!ActorInfo || !Super::CanActivateAbility(
		Handle,
		ActorInfo,
		SourceTags,
		TargetTags,
		OptionalRelevantTags))
	{
		return false;
	}

	const AActor* Relic = Cast<AActor>(GetSourceObject(Handle, ActorInfo));
	const UGrabbableComponent* Grabbable = Relic
		? Relic->FindComponentByClass<UGrabbableComponent>()
		: nullptr;
	const bool bSupportsAimView = Relic
		&& (Relic->FindComponentByClass<UNPAimableRelicComponent>()
			|| Relic->FindComponentByClass<UNPThrowableRelicComponent>());
	return bSupportsAimView
		&& Grabbable
		&& Grabbable->GetActiveGrabCount() == 1
		&& Cast<ANPStablePhysicsPawn>(ActorInfo->AvatarActor.Get());
}

void UNPRelicAimAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AActor* Relic = Cast<AActor>(GetCurrentSourceObject());
	ANPStablePhysicsPawn* Pawn = ActorInfo
		? Cast<ANPStablePhysicsPawn>(ActorInfo->AvatarActor.Get())
		: nullptr;
	UGrabbableComponent* Grabbable = Relic
		? Relic->FindComponentByClass<UGrabbableComponent>()
		: nullptr;
	if (!Pawn
		|| !Grabbable
		|| Grabbable->GetActiveGrabCount() != 1
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AimPawn = Pawn;
	AimGrabbable = Grabbable;
	AimGrabbable->OnActiveGrabCountChanged.AddUObject(
		this,
		&UNPRelicAimAbility::HandleGrabCountChanged);
	Pawn->SetRelicAimViewActive(true);
}

void UNPRelicAimAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	if (AimGrabbable.IsValid())
	{
		AimGrabbable->OnActiveGrabCountChanged.RemoveAll(this);
	}
	AimGrabbable.Reset();

	if (AimPawn.IsValid())
	{
		AimPawn->SetRelicAimViewActive(false);
	}
	AimPawn.Reset();

	Super::EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		bReplicateEndAbility,
		bWasCancelled);
}

void UNPRelicAimAbility::HandleGrabCountChanged(const int32 ActiveGrabCount)
{
	if (ActiveGrabCount != 1)
	{
		K2_CancelAbility();
	}
}
