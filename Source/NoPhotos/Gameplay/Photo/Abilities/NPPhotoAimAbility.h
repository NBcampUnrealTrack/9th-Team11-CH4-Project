#pragma once

#include "Abilities/GameplayAbility.h"
#include "NPPhotoAimAbility.generated.h"

class UNPPhotoCaptureComponent;

/** 사진 모드의 진입부터 종료까지 상태와 카메라 수명을 소유하는 Ability입니다. */
UCLASS()
class NOPHOTOS_API UNPPhotoAimAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UNPPhotoAimAbility();

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
	UNPPhotoCaptureComponent* ResolvePhotoCaptureComponent(
		const FGameplayAbilityActorInfo* ActorInfo) const;

	TWeakObjectPtr<UNPPhotoCaptureComponent> ActivePhotoCaptureComponent;
};
