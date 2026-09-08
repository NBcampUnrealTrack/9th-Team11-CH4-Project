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

	/** 조준 입력 라우터가 호출합니다. */
	void ActivateRelicAimAbility();
	void CancelRelicAimAbility();
	void ActivateRelicFireAbility();
	void TogglePhotoAimAbility();
	void CancelPhotoAimAbility();
	void ActivatePhotoShotAbility();

private:
	void ActivateRelicUseAbility();
	void ClearHeldRelicAbilities();
	void HandleGameplayEffectApplied(
		UAbilitySystemComponent* SourceAbilitySystem,
		const FGameplayEffectSpec& EffectSpec,
		FActiveGameplayEffectHandle ActiveHandle);
	void HandleKnockbackEffect(const FGameplayEffectSpec& EffectSpec);

	TArray<FGameplayAbilitySpecHandle> HeldRelicAbilityHandles;
	TArray<FGameplayAbilitySpecHandle> DefaultPhotoAbilityHandles;
	bool bGameplayEffectDelegateBound = false;
	bool bDefaultPhotoAbilitiesGranted = false;
};
