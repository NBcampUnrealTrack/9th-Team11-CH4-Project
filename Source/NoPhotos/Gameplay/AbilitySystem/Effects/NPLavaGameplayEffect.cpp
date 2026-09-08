#include "Gameplay/AbilitySystem/Effects/NPLavaGameplayEffect.h"

#include "Core/GameplayTag/NPGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UNPLavaGameplayEffect::UNPLavaGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FScalableFloat(0.5f);
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;

	UTargetTagsGameplayEffectComponent* TargetTags =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("LavaTargetTags"));
	GEComponents.Add(TargetTags);
	FInheritedTagContainer GrantedTags;
	GrantedTags.AddTag(NPGameplayTags::State_LavaBurning);
	TargetTags->SetAndApplyTargetTagChanges(GrantedTags);
	GameplayCues.Emplace(NPGameplayTags::GameplayCue_Status_LavaBurning, 0.0f, 1.0f);
}
