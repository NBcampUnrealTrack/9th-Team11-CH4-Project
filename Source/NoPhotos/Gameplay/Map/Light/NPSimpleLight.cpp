#include "Gameplay/Map/Light/NPSimpleLight.h"

#include "Gameplay/Map/Light/Components/NPEmissiveLightComponent.h"

ANPSimpleLight::ANPSimpleLight()
{
	EmissiveLight = CreateDefaultSubobject<UNPEmissiveLightComponent>(
		TEXT("EmissiveLight"));
	EmissiveLight->SetupAttachment(SceneRoot);
}

void ANPSimpleLight::ApplyLightState()
{
	Super::ApplyLightState();

	if (IsValid(EmissiveLight))
	{
		EmissiveLight->SetLightEnabled(bLightEnabled);
	}
}
