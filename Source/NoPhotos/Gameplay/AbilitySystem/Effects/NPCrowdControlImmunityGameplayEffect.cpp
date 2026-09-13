#include "NPCrowdControlImmunityGameplayEffect.h"

#include "Core/GameplayTag/NPGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UNPCrowdControlImmunityGameplayEffect::UNPCrowdControlImmunityGameplayEffect(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	UTargetTagsGameplayEffectComponent* TargetTags =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
			TEXT("CrowdControlImmunityTargetTags"));
	GEComponents.Add(TargetTags);

	FInheritedTagContainer GrantedTags;
	GrantedTags.AddTag(NPGameplayTags::State_CrowdControl_Immune);
	TargetTags->SetAndApplyTargetTagChanges(GrantedTags);
}
