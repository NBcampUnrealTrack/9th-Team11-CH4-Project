#include "Gameplay/AbilitySystem/Effects/NPInvisibilityGameplayEffect.h"

#include "Core/GameplayTag/NPGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UNPInvisibilityGameplayEffect::UNPInvisibilityGameplayEffect(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	// 서로 다른 이벤트/유물이 부여한 효과를 각각의 핸들로 제거할 수 있게 합니다.
	StackingType = EGameplayEffectStackingType::None;

	UTargetTagsGameplayEffectComponent* TargetTags =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("InvisibilityTargetTags"));
	GEComponents.Add(TargetTags);

	FInheritedTagContainer GrantedTags;
	GrantedTags.AddTag(NPGameplayTags::State_Invisible);
	GrantedTags.AddTag(NPGameplayTags::State_VisionRestricted);
	TargetTags->SetAndApplyTargetTagChanges(GrantedTags);
}
