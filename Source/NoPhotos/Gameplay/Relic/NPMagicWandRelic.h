#pragma once

#include "Gameplay/Relic/NPBaseRelic.h"
#include "NPMagicWandRelic.generated.h"

class UNPFireballRelicComponent;

UCLASS(Blueprintable)
class NOPHOTOS_API ANPMagicWandRelic : public ANPBaseRelic
{
	GENERATED_BODY()

public:
	ANPMagicWandRelic(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UNPFireballRelicComponent> FireballRelicComponent;
};
