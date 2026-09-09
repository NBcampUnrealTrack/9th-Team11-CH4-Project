#include "Gameplay/Character/Abilities/NPScanAbility.h"

#include "AbilitySystemComponent.h"
#include "Core/GameplayTag/NPGameplayTags.h"
#include "Engine/World.h"
#include "Gameplay/Character/NPStablePhysicsPawn.h"
#include "GameplayAbilitySpec.h"

UNPScanAbility::UNPScanAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer Tags;
	Tags.AddTag(NPGameplayTags::Input_Scan);
	Tags.AddTag(NPGameplayTags::Ability_Scan);
	SetAssetTags(Tags);

	ActivationBlockedTags.AddTag(NPGameplayTags::State_Photo_Aiming);
	ActivationBlockedTags.AddTag(NPGameplayTags::State_Relic_Aiming);
	ActivationBlockedTags.AddTag(
		NPGameplayTags::State_Relic_Carrying_Usable);
	ActivationBlockedTags.AddTag(
		NPGameplayTags::State_CrowdControl_Stunned);
}

bool UNPScanAbility::CanActivateAbility(
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

	const UWorld* World = ActorInfo->AvatarActor.IsValid()
		? ActorInfo->AvatarActor->GetWorld()
		: nullptr;
	UAbilitySystemComponent* AbilitySystem =
		ActorInfo->AbilitySystemComponent.Get();
	if (!World
		|| !AbilitySystem
		|| World->GetTimeSeconds() < NextScanAllowedTime)
	{
		return false;
	}

	FGameplayTagContainer RelicAbilityTags;
	RelicAbilityTags.AddTag(NPGameplayTags::Ability_Relic);
	TArray<FGameplayAbilitySpec*> RelicAbilitySpecs;
	AbilitySystem->GetActivatableGameplayAbilitySpecsByAllMatchingTags(
		RelicAbilityTags,
		RelicAbilitySpecs,
		false);
	for (const FGameplayAbilitySpec* AbilitySpec : RelicAbilitySpecs)
	{
		if (AbilitySpec && AbilitySpec->IsActive())
		{
			return false;
		}
	}

	return Cast<ANPStablePhysicsPawn>(ActorInfo->AvatarActor.Get()) != nullptr;
}

void UNPScanAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		TriggerEventData);

	ANPStablePhysicsPawn* Pawn = ActorInfo
		? Cast<ANPStablePhysicsPawn>(ActorInfo->AvatarActor.Get())
		: nullptr;
	UWorld* World = Pawn ? Pawn->GetWorld() : nullptr;
	if (!Pawn || !World)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	NextScanAllowedTime = World->GetTimeSeconds()
		+ FMath::Max(0.0f, ScanCooldown);
	Pawn->TriggerScanPresentation();
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
