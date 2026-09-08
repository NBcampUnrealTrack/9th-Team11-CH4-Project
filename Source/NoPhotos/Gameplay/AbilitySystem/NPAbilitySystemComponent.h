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
	float GetLavaBurnDuration() const { return LavaBurnDuration; }

protected:
	/** 용암 접촉 시 설정할 월드 Z축 속도입니다. 수평 속도는 유지합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lava", meta=(ClampMin="0.0", Units="cm/s"))
	float LavaJumpVelocity = 1000.0f;

	/** 용암 상태와 두 불 나이아가라가 유지되는 시간입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Lava",
		meta=(DisplayName="불 연출 지속 시간", ClampMin="0.01", Units="s"))
	float LavaBurnDuration = 0.5f;
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
