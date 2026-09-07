#include "Gameplay/Photo/NPPhotoStunVisualComponent.h"

#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

UNPPhotoStunVisualComponent::UNPPhotoStunVisualComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	SetCastShadow(false);
	SetVisibility(false, true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshFinder.Succeeded())
	{
		SetStaticMesh(CubeMeshFinder.Object);
	}
}

void UNPPhotoStunVisualComponent::OnRegister()
{
	Super::OnRegister();
	RebuildMarkerInstances();
	SetVisibility(bStunVisualActive, true);
	SetComponentTickEnabled(bStunVisualActive);
}

void UNPPhotoStunVisualComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bStunVisualActive || DeltaTime <= 0.0f)
	{
		return;
	}

	AddLocalRotation(FRotator(
		0.0f,
		RotationSpeedDegrees * DeltaTime,
		0.0f));
}

void UNPPhotoStunVisualComponent::SetStunVisualActive(const bool bActive)
{
	bStunVisualActive = bActive;
	SetVisibility(bActive, true);
	SetComponentTickEnabled(bActive);
}

void UNPPhotoStunVisualComponent::RebuildMarkerInstances()
{
	ClearInstances();
	for (int32 MarkerIndex = 0; MarkerIndex < 3; ++MarkerIndex)
	{
		const float AngleRadians = FMath::DegreesToRadians(
			120.0f * static_cast<float>(MarkerIndex));
		const FVector Location(
			FMath::Cos(AngleRadians) * OrbitRadius,
			FMath::Sin(AngleRadians) * OrbitRadius,
			0.0f);
		AddInstance(FTransform(FRotator::ZeroRotator, Location, MarkerScale));
	}
}
