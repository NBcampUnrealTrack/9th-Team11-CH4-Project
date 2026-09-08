#include "Gameplay/AbilitySystem/Effects/NPLeaderGameplayEffect.h"

#include "Core/GameplayTag/NPGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UNPLeaderGameplayEffect::UNPLeaderGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	UTargetTagsGameplayEffectComponent* TargetTags =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("LeaderTargetTags"));
	GEComponents.Add(TargetTags);
	FInheritedTagContainer GrantedTags;
	GrantedTags.AddTag(NPGameplayTags::State_Ranking_Leader);
	TargetTags->SetAndApplyTargetTagChanges(GrantedTags);
}
