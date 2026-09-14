#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Abilities/GameplayAbility.h"
#include "NPUsableRelicComponent.generated.h"

UCLASS(ClassGroup=(Relic), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPUsableRelicComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNPUsableRelicComponent();
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	const TArray<TSubclassOf<UGameplayAbility>>& GetUseAbilityClasses() const
	{
		return UseAbilityClasses;
	}

	/** 서버가 확정한 현재 사용 쿨타임을 로컬 UI용 값으로 반환합니다. */
	bool GetCooldownDisplay(
		float& OutProgress,
		float& OutRemainingTime,
		float& OutDuration) const;

protected:
	/** 실제 사용이 승인된 시점에 서버에서 호출합니다. */
	void StartUseCooldown(float Duration);

	void SetUseAbilityClass(TSubclassOf<UGameplayAbility> InAbilityClass);
	void SetUseAbilityClasses(
		const TArray<TSubclassOf<UGameplayAbility>>& InAbilityClasses);

private:
	UPROPERTY(Replicated)
	float CooldownEndServerTime = 0.0f;

	UPROPERTY(Replicated)
	float CooldownDuration = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Relic Ability", meta=(AllowPrivateAccess="true"))
	TArray<TSubclassOf<UGameplayAbility>> UseAbilityClasses;
};
