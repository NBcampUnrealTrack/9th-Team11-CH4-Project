#include "Gameplay/AbilitySystem/GameplayCue/NPPhotoAimStartGameplayCue.h"

#include "Core/GameplayTag/NPGameplayTags.h"

UNPPhotoAimStartGameplayCue::UNPPhotoAimStartGameplayCue()
{
	GameplayCueTag = NPGameplayTags::GameplayCue_Photo_AimStart;
	GameplayCueName = GameplayCueTag.GetTagName();
}

bool UNPPhotoAimStartGameplayCue::OnExecute_Implementation(
	AActor* Target,
	const FGameplayCueParameters& Parameters) const
{
	Super::OnExecute_Implementation(Target, Parameters);
	return true;
}
