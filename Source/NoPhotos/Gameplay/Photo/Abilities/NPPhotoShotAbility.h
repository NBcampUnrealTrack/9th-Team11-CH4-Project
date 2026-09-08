#pragma once

#include "Abilities/GameplayAbility.h"
#include "NPPhotoShotAbility.generated.h"

/** 실제 Scene Capture 작업을 PhotoCaptureComponent에 위임하는 단발 Ability입니다. */
UCLASS()
class NOPHOTOS_API UNPPhotoShotAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UNPPhotoShotAbility();

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
