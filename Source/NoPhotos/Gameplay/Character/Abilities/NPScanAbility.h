#pragma once

#include "Abilities/GameplayAbility.h"
#include "NPScanAbility.generated.h"

/** 스캔 입력의 사용 조건과 재사용 대기시간을 관리합니다. */
UCLASS(Blueprintable, BlueprintType)
class NOPHOTOS_API UNPScanAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UNPScanAbility();

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Scan", meta=(ClampMin="0.0", Units="s"))
	float ScanCooldown = 2.0f;

private:
	double NextScanAllowedTime = 0.0;
};
