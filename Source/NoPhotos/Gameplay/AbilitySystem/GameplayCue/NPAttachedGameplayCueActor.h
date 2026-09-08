#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "NPAttachedGameplayCueActor.generated.h"

class USceneComponent;

/** 캐릭터에 붙는 Gameplay Cue의 공통 기반입니다. */
UCLASS(Abstract, Blueprintable)
class NOPHOTOS_API ANPAttachedGameplayCueActor : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	ANPAttachedGameplayCueActor();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gameplay Cue")
	TObjectPtr<USceneComponent> SceneRoot;
};
