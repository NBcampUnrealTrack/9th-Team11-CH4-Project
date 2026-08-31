#include "Gameplay/Map/Light/NPVolumetricSpotLight.h"

#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

#if WITH_EDITOR
#include "UObject/UnrealType.h"
#endif

ANPVolumetricSpotLight::ANPVolumetricSpotLight()
{
	BeamMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BeamMesh"));
	BeamMesh->SetupAttachment(SceneRoot);
}

void ANPVolumetricSpotLight::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	DynamicMaterial = nullptr;
	ApplyMaterialParameters();
}

#if WITH_EDITOR
void ANPVolumetricSpotLight::PostEditChangeProperty(
	FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	NearFadeDistance = FMath::Max(0.0f, NearFadeDistance);
	Opacity = FMath::Clamp(Opacity, 0.0f, 1.0f);
	DynamicMaterial = nullptr;
	ApplyMaterialParameters();
}
#endif

void ANPVolumetricSpotLight::ApplyLightState()
{
	Super::ApplyLightState();

	if (IsValid(BeamMesh))
	{
		BeamMesh->SetVisibility(bLightEnabled);
	}
}

void ANPVolumetricSpotLight::ApplyMaterialParameters()
{
	if (!IsValid(BeamMesh) || !IsValid(BeamMaterial))
	{
		return;
	}

	if (!IsValid(DynamicMaterial))
	{
		DynamicMaterial = BeamMesh->CreateDynamicMaterialInstance(
			0,
			BeamMaterial);
	}

	if (!IsValid(DynamicMaterial))
	{
		return;
	}

	DynamicMaterial->SetScalarParameterValue(
		TEXT("NearFadeDistance"),
		NearFadeDistance);
	DynamicMaterial->SetScalarParameterValue(TEXT("Opacity"), Opacity);
	DynamicMaterial->SetVectorParameterValue(TEXT("Color"), Color);

	if (IsValid(FalloffTexture))
	{
		DynamicMaterial->SetTextureParameterValue(
			TEXT("FalloffTexture"),
			FalloffTexture);
	}
}
