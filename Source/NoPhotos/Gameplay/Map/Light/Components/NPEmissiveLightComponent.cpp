#include "Gameplay/Map/Light/Components/NPEmissiveLightComponent.h"

#include "Materials/MaterialInstanceDynamic.h"

UNPEmissiveLightComponent::UNPEmissiveLightComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UNPEmissiveLightComponent::SetLightEnabled(const bool bEnabled)
{
	bLightEnabled = bEnabled;
	ApplyLightState();
}

void UNPEmissiveLightComponent::BeginPlay()
{
	Super::BeginPlay();

	if (MaterialIndex >= 0)
	{
		DynamicMaterialInstance = CreateDynamicMaterialInstance(MaterialIndex);
	}

	ApplyLightState();
}

void UNPEmissiveLightComponent::ApplyLightState()
{
	if (IsValid(DynamicMaterialInstance) &&
		!IntensityParameterName.IsNone())
	{
		DynamicMaterialInstance->SetScalarParameterValue(
			IntensityParameterName,
			bLightEnabled ? 1.0f : 0.0f);
	}
}
