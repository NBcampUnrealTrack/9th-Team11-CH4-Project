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

	// 이 무한 GE가 적용된 동안 Cue의 OnActive/WhileActive를 유지하고,
	// GE가 제거되면 Removed를 보내 머리 위 연출도 함께 정리합니다.
	GameplayCues.Emplace(NPGameplayTags::GameplayCue_Status_ControlReversal, 0.0f, 1.0f);
}
