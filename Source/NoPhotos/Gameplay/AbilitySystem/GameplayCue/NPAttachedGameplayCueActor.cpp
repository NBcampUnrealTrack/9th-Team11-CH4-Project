#include "Gameplay/AbilitySystem/GameplayCue/NPAttachedGameplayCueActor.h"

#include "Components/SceneComponent.h"

ANPAttachedGameplayCueActor::ANPAttachedGameplayCueActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	bAutoAttachToOwner = true;
	bAutoDestroyOnRemove = true;
	bAllowMultipleOnActiveEvents = false;
	bAllowMultipleWhileActiveEvents = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}
