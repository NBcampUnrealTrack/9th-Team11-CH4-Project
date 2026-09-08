#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Relic/Gimmick/Components/NPRelicGimmickComponent.h"
#include "NPShipGimmickComponent.generated.h"

/** Common completion state used only by the ship devices. */
UCLASS(ClassGroup=(Relic), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPShipGimmickComponent : public UNPRelicGimmickComponent
{
	GENERATED_BODY()
};
