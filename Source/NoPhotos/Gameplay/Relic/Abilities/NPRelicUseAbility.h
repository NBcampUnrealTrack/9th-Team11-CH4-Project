#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "NPRelicUseAbility.generated.h"

class ANPStablePhysicsPawn;
class UNPSwingableRelicComponent;
class UGrabbableComponent;

UCLASS()
class NOPHOTOS_API UNPRelicUseAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UNPRelicUseAbility();

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
	UFUNCTION()
	void HandleSwingFinished();

	TWeakObjectPtr<ANPStablePhysicsPawn> SwingPawn;
	TWeakObjectPtr<UNPSwingableRelicComponent> SwingableRelicComponent;
	TWeakObjectPtr<UGrabbableComponent> LockedGrabbableComponent;
	bool bSwingStarted = false;
	bool bGrabLockAcquired = false;
};
