#pragma once

#include "Abilities/GameplayAbility.h"
#include "NPRelicAimAbility.generated.h"

class ANPStablePhysicsPawn;
class UGrabbableComponent;

UCLASS()
class NOPHOTOS_API UNPRelicAimAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UNPRelicAimAbility();

protected:
	virtual bool CanActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

private:
	void HandleGrabCountChanged(int32 ActiveGrabCount);

	TWeakObjectPtr<ANPStablePhysicsPawn> AimPawn;
	TWeakObjectPtr<UGrabbableComponent> AimGrabbable;
	bool bAimingTagAdded = false;
};
