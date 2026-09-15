#include "Gameplay/Relic/Abilities/NPThrowableRelicUseAbility.h"

#include "Core/GameplayTag/NPGameplayTags.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/Character/NPReplicatedStablePhysicsPawn.h"
#include "Gameplay/Relic/Components/NPThrowableRelicComponent.h"

UNPThrowableRelicUseAbility::UNPThrowableRelicUseAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	bServerRespectsRemoteAbilityCancellation = false;

	FGameplayTagContainer Tags;
	Tags.AddTag(NPGameplayTags::Input_Relic_Use);
	Tags.AddTag(NPGameplayTags::Ability_Relic);
	SetAssetTags(Tags);
}

bool UNPThrowableRelicUseAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!ActorInfo
		|| !Super::CanActivateAbility(
			Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	const AActor* Relic = Cast<AActor>(GetSourceObject(Handle, ActorInfo));
	const UNPThrowableRelicComponent* Throwable = Relic
		? Relic->FindComponentByClass<UNPThrowableRelicComponent>()
		: nullptr;
	const ANPStablePhysicsPawn* Pawn =
		Cast<ANPStablePhysicsPawn>(ActorInfo->AvatarActor.Get());
	return Throwable && Throwable->CanThrow(Pawn);
}

void UNPThrowableRelicUseAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	ANPReplicatedStablePhysicsPawn* Pawn = ActorInfo
		? Cast<ANPReplicatedStablePhysicsPawn>(ActorInfo->AvatarActor.Get())
		: nullptr;
	bool bRequestedThrow = false;
	if (Pawn && ActorInfo && ActorInfo->IsLocallyControlled())
	{
		if (APlayerController* PlayerController = Cast<APlayerController>(
			Pawn->GetController()))
		{
			FVector CameraLocation;
			FRotator CameraRotation;
			PlayerController->GetPlayerViewPoint(
				CameraLocation,
				CameraRotation);
			Pawn->ServerRequestThrowableRelicThrow(
				CameraLocation,
				CameraRotation.Vector());
			bRequestedThrow = true;
		}
	}
	if (IsActive())
	{
		EndAbility(
			Handle,
			ActorInfo,
			ActivationInfo,
			true,
			!bRequestedThrow && ActorInfo && ActorInfo->IsLocallyControlled());
	}
}

