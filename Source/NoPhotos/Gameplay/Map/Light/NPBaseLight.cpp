#include "Gameplay/Map/Light/NPBaseLight.h"

#include "Components/SceneComponent.h"

ANPBaseLight::ANPBaseLight()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void ANPBaseLight::SetLightEnabled(const bool bEnabled)
{
	bLightEnabled = bEnabled;
	ApplyLightState();
}

void ANPBaseLight::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyLightState();
}

#if WITH_EDITOR
void ANPBaseLight::PostEditChangeProperty(
	FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	ApplyLightState();
}
#endif

void ANPBaseLight::ApplyLightState()
{
}
