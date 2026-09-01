#include "Gameplay/Relic/NPMagicWandRelic.h"

#include "Gameplay/Relic/Components/NPFireballRelicComponent.h"

ANPMagicWandRelic::ANPMagicWandRelic(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FireballRelicComponent =
		CreateDefaultSubobject<UNPFireballRelicComponent>(
			TEXT("FireballRelicComponent"));
}
