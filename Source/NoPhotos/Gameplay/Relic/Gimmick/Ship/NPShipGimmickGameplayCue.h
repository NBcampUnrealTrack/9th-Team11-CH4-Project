#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "NPShipGimmickGameplayCue.generated.h"

UCLASS(Blueprintable)
class NOPHOTOS_API ANPShipGimmickGameplayCue : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	ANPShipGimmickGameplayCue();

protected:
	virtual bool OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) override;
};
