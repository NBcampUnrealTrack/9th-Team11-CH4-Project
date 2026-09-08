#include "Gameplay/AbilitySystem/Effects/NPPhotoCooldownGameplayEffect.h"

#include "Core/GameplayTag/NPGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UNPPhotoCooldownGameplayEffect::UNPPhotoCooldownGameplayEffect(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FScalableFloat(5.0f);
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	StackLimitCount = 1;
	StackDurationRefreshPolicy =
		EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;

	UTargetTagsGameplayEffectComponent* TargetTags =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
			TEXT("PhotoCooldownTargetTags"));
	GEComponents.Add(TargetTags);
	FInheritedTagContainer GrantedTags;
	GrantedTags.AddTag(NPGameplayTags::Cooldown_Photo_Shot);
	TargetTags->SetAndApplyTargetTagChanges(GrantedTags);
}
