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

	const TArray<TSubclassOf<UGameplayAbility>>& GetUseAbilityClasses() const
	{
		return UseAbilityClasses;
	}

protected:
	void SetUseAbilityClass(TSubclassOf<UGameplayAbility> InAbilityClass);
	void SetUseAbilityClasses(
		const TArray<TSubclassOf<UGameplayAbility>>& InAbilityClasses);

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Relic Ability", meta=(AllowPrivateAccess="true"))
	TArray<TSubclassOf<UGameplayAbility>> UseAbilityClasses;
};
