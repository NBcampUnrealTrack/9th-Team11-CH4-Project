#pragma once

#include "Abilities/GameplayAbility.h"
#include "NPThrowableRelicUseAbility.generated.h"

UCLASS()
class NOPHOTOS_API UNPThrowableRelicUseAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UNPThrowableRelicUseAbility();

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
};

