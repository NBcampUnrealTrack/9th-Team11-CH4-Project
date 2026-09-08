#include "Gameplay/AbilitySystem/Effects/NPPossessionGameplayEffect.h"

#include "Core/GameplayTag/NPGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UNPPossessionGameplayEffect::UNPPossessionGameplayEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	StackingType = EGameplayEffectStackingType::None;
	UTargetTagsGameplayEffectComponent* TargetTags =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("PossessionTargetTags"));
	GEComponents.Add(TargetTags);
	FInheritedTagContainer GrantedTags;
	GrantedTags.AddTag(NPGameplayTags::State_ControlsMirrored);
	TargetTags->SetAndApplyTargetTagChanges(GrantedTags);
	GameplayCues.Emplace(NPGameplayTags::GameplayCue_Status_ControlReversal, 0.0f, 1.0f);
}
