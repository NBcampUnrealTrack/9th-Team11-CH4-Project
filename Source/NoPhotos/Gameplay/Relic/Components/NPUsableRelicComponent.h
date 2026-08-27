#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NPUsableRelicComponent.generated.h"

class UGameplayAbility;
UCLASS(ClassGroup=(Relic), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPUsableRelicComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNPUsableRelicComponent();

	TSubclassOf<UGameplayAbility> GetUseAbilityClass() const
	{
		return UseAbilityClass;
	}

protected:
	void SetUseAbilityClass(TSubclassOf<UGameplayAbility> InAbilityClass)
	{
		UseAbilityClass = InAbilityClass;
	}

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Relic Ability", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UGameplayAbility> UseAbilityClass;
};
