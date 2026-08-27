#include "Gameplay/AbilitySystem/Effects/NPKnockbackGameplayEffect.h"

UNPKnockbackGameplayEffect::UNPKnockbackGameplayEffect(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
}
