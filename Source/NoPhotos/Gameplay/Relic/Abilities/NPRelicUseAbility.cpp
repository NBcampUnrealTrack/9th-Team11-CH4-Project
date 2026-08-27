#include "Gameplay/Relic/Abilities/NPRelicUseAbility.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Core/GameplayTag/NPGameplayTags.h"
#include "Engine/Engine.h"
#include "Gameplay/Character/NPStablePhysicsPawn.h"
#include "Gameplay/Interaction/Components/GrabbableComponent.h"
#include "Gameplay/Relic/Components/NPSwingableRelicComponent.h"

UNPRelicUseAbility::UNPRelicUseAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer Tags;
	Tags.AddTag(NPGameplayTags::Input_Relic_Use);
	Tags.AddTag(NPGameplayTags::Ability_Relic);
	SetAssetTags(Tags);
}

void UNPRelicUseAbility::ActivateAbility(
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

	AActor* Relic = Cast<AActor>(GetCurrentSourceObject());
	UNPSwingableRelicComponent* SwingableRelic = Relic
		? Relic->FindComponentByClass<UNPSwingableRelicComponent>()
		: nullptr;
	const UGrabbableComponent* GrabbableRelic = Relic
		? Relic->FindComponentByClass<UGrabbableComponent>()
		: nullptr;
	ANPStablePhysicsPawn* Pawn = ActorInfo
		? Cast<ANPStablePhysicsPawn>(ActorInfo->AvatarActor.Get())
		: nullptr;
	if (!SwingableRelic
		|| !GrabbableRelic
		|| GrabbableRelic->GetActiveGrabCount() != 1
		|| !Pawn
		|| !Pawn->BeginRelicSwing(SwingableRelic->GetSwingSettings()))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bSwingStarted = true;
	SwingPawn = Pawn;
	SwingableRelicComponent = SwingableRelic;
	if (SwingableRelicComponent.IsValid() && ActorInfo)
	{
		SwingableRelicComponent->BeginHitWindow(
			Pawn,
			ActorInfo->AbilitySystemComponent.Get(),
			Pawn->GetViewForwardDirection());
	}

	if (ActorInfo && ActorInfo->IsLocallyControlled() && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			2.0f,
			FColor::Yellow,
			TEXT("Relic Swing Started"));
	}

	UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(
		this,
		FMath::Max(SwingableRelic->GetSwingSettings().Duration, 0.01f));
	WaitTask->OnFinish.AddDynamic(
		this,
		&UNPRelicUseAbility::HandleSwingFinished);
	WaitTask->ReadyForActivation();
}

void UNPRelicUseAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (SwingableRelicComponent.IsValid())
	{
		SwingableRelicComponent->EndHitWindow();
	}
	SwingableRelicComponent.Reset();

	if (bSwingStarted && SwingPawn.IsValid())
	{
		SwingPawn->EndRelicSwing();
	}
	bSwingStarted = false;
	SwingPawn.Reset();

	Super::EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		bReplicateEndAbility,
		bWasCancelled);
}

void UNPRelicUseAbility::HandleSwingFinished()
{
	K2_EndAbility();
}
