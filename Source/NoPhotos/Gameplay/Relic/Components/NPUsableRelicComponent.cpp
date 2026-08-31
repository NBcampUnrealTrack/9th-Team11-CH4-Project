#include "Gameplay/Relic/Components/NPUsableRelicComponent.h"

#include "Abilities/GameplayAbility.h"

UNPUsableRelicComponent::UNPUsableRelicComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UNPUsableRelicComponent::SetUseAbilityClass(
	TSubclassOf<UGameplayAbility> InAbilityClass)
{
	UseAbilityClasses.Reset();
	if (InAbilityClass)
	{
		UseAbilityClasses.Add(InAbilityClass);
	}
}

void UNPUsableRelicComponent::SetUseAbilityClasses(
	const TArray<TSubclassOf<UGameplayAbility>>& InAbilityClasses)
{
	UseAbilityClasses = InAbilityClasses;
}
