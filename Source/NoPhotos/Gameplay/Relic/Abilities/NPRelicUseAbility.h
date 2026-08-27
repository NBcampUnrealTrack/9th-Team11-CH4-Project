#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "NPRelicUseAbility.generated.h"

class ANPStablePhysicsPawn;
class UNPSwingableRelicComponent;

UCLASS()
class NOPHOTOS_API UNPRelicUseAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UNPRelicUseAbility();

protected:
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
	bool bSwingStarted = false;
};
