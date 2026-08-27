#pragma once

#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpecHandle.h"
#include "NPAbilitySystemComponent.generated.h"

class UEnhancedInputComponent;
class UInputAction;

UCLASS(ClassGroup=(Abilities), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UNPAbilitySystemComponent();

	void InitializeForOwner();
	void BindRelicUseInput(
		UEnhancedInputComponent* EnhancedInputComponent,
		UInputAction* RelicUseAction);
	void SetHeldRelic(AActor* Relic);

private:
	void ActivateRelicUseAbility();
	void ClearHeldRelicAbility();
	void HandleGameplayEffectApplied(
		UAbilitySystemComponent* SourceAbilitySystem,
		const FGameplayEffectSpec& EffectSpec,
		FActiveGameplayEffectHandle ActiveHandle);

	FGameplayAbilitySpecHandle HeldRelicAbilityHandle;
	bool bGameplayEffectDelegateBound = false;
};
