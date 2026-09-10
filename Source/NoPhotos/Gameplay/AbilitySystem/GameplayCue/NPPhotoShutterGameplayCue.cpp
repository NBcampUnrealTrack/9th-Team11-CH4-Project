#include "Gameplay/AbilitySystem/GameplayCue/NPPhotoShutterGameplayCue.h"

#include "Core/GameplayTag/NPGameplayTags.h"

UNPPhotoShutterGameplayCue::UNPPhotoShutterGameplayCue()
{
	GameplayCueTag = NPGameplayTags::GameplayCue_Photo_Shutter;
	GameplayCueName = GameplayCueTag.GetTagName();
}

bool UNPPhotoShutterGameplayCue::OnExecute_Implementation(
	AActor* Target,
	const FGameplayCueParameters& Parameters) const
{
	Super::OnExecute_Implementation(Target, Parameters);
	return true;
}
