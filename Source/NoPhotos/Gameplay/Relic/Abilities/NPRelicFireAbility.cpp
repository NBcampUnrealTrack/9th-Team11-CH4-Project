#include "Gameplay/Relic/Abilities/NPRelicFireAbility.h"

#include "AbilitySystemComponent.h"
#include "Core/GameplayTag/NPGameplayTags.h"
#include "Gameplay/Character/NPReplicatedStablePhysicsPawn.h"
#include "Gameplay/Interaction/Components/GrabbableComponent.h"
#include "Gameplay/Relic/Components/NPAimableRelicComponent.h"
#include "GameFramework/PlayerController.h"

UNPRelicFireAbility::UNPRelicFireAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer Tags;
	Tags.AddTag(NPGameplayTags::Input_Relic_Fire);
	Tags.AddTag(NPGameplayTags::Ability_Relic);
	Tags.AddTag(NPGameplayTags::Ability_Relic_Fire);
	SetAssetTags(Tags);
}

bool UNPRelicFireAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!ActorInfo
		|| !ActorInfo->AbilitySystemComponent.IsValid()
		|| !ActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(
			NPGameplayTags::State_Relic_Aiming)
		|| !Super::CanActivateAbility(
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
	return Relic
		&& Relic->FindComponentByClass<UNPAimableRelicComponent>()
		&& Grabbable
		&& Grabbable->GetActiveGrabCount() == 1
		&& Cast<ANPReplicatedStablePhysicsPawn>(ActorInfo->AvatarActor.Get());
}

void UNPRelicFireAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AActor* Relic = Cast<AActor>(GetCurrentSourceObject());
	UNPAimableRelicComponent* AimableRelic = Relic
		? Relic->FindComponentByClass<UNPAimableRelicComponent>()
		: nullptr;
	ANPReplicatedStablePhysicsPawn* Pawn = ActorInfo
		? Cast<ANPReplicatedStablePhysicsPawn>(ActorInfo->AvatarActor.Get())
		: nullptr;
	if (!AimableRelic
		|| !Pawn
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (ActorInfo && ActorInfo->IsLocallyControlled())
	{
		APlayerController* PlayerController = Cast<APlayerController>(
			Pawn->GetController());
		if (!PlayerController)
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}

		FVector CameraLocation;
		FRotator CameraRotation;
		PlayerController->GetPlayerViewPoint(
			CameraLocation,
			CameraRotation);
		Pawn->ServerRequestAimableRelicFire(
			CameraLocation,
			CameraRotation.Vector());
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
