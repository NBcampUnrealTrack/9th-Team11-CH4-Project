#include "Gameplay/AbilitySystem/Effects/NPPhotoStunGameplayEffect.h"

#include "Core/GameplayTag/NPGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UNPPhotoStunGameplayEffect::UNPPhotoStunGameplayEffect(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FScalableFloat(1.0f);
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	StackLimitCount = 1;
	StackDurationRefreshPolicy =
		EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;

	UTargetTagsGameplayEffectComponent* TargetTags =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
			TEXT("PhotoStunTargetTags"));
	GEComponents.Add(TargetTags);
	FInheritedTagContainer GrantedTags;
	GrantedTags.AddTag(NPGameplayTags::State_CrowdControl_Stunned);
	TargetTags->SetAndApplyTargetTagChanges(GrantedTags);
}
