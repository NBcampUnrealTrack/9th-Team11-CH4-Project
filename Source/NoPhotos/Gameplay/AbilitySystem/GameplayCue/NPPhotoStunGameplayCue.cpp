#include "Gameplay/AbilitySystem/GameplayCue/NPPhotoStunGameplayCue.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ANPPhotoStunGameplayCue::ANPPhotoStunGameplayCue()
{
	SceneRoot->SetAbsolute(false, true, false);

	MarkerMesh = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("MarkerMesh"));
	MarkerMesh->SetupAttachment(SceneRoot);
	MarkerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MarkerMesh->SetGenerateOverlapEvents(false);
	MarkerMesh->SetCanEverAffectNavigation(false);
	MarkerMesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshFinder.Succeeded())
	{
		MarkerMesh->SetStaticMesh(CubeMeshFinder.Object);
	}
}

void ANPPhotoStunGameplayCue::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildMarkerInstances();
}

void ANPPhotoStunGameplayCue::PrepareVisual()
{
	SceneRoot->SetWorldRotation(FRotator::ZeroRotator);
	MarkerMesh->SetRelativeRotation(FRotator::ZeroRotator);
	RebuildMarkerInstances();
}

void ANPPhotoStunGameplayCue::ResetVisual()
{
	if (MarkerMesh)
	{
		MarkerMesh->SetRelativeRotation(FRotator::ZeroRotator);
		MarkerMesh->ClearInstances();
	}
}

void ANPPhotoStunGameplayCue::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!IsActorTickEnabled())
	{
		return;
	}
	MarkerMesh->AddLocalRotation(FRotator(
		0.0f,
		RotationSpeedDegrees * FMath::Max(DeltaSeconds, 0.0f),
		0.0f));
}

void ANPPhotoStunGameplayCue::ApplyVisualScale()
{
	UpdateMarkerInstances();
}

void ANPPhotoStunGameplayCue::RebuildMarkerInstances()
{
	MarkerMesh->ClearInstances();
	MarkerMesh->SetRelativeLocation(FVector(0.0f, 0.0f, VisualHeight));
	for (int32 MarkerIndex = 0; MarkerIndex < 3; ++MarkerIndex)
	{
		const float AngleRadians = FMath::DegreesToRadians(
			120.0f * static_cast<float>(MarkerIndex));
		const FVector Location(
			FMath::Cos(AngleRadians) * OrbitRadius,
			FMath::Sin(AngleRadians) * OrbitRadius,
			0.0f);
		MarkerMesh->AddInstance(
			FTransform(FRotator::ZeroRotator, Location,
				MarkerScale * GetVisualScaleMultiplier()));
	}
}

void ANPPhotoStunGameplayCue::UpdateMarkerInstances()
{
	for (int32 MarkerIndex = 0; MarkerIndex < 3; ++MarkerIndex)
	{
		const float AngleRadians = FMath::DegreesToRadians(
			120.0f * static_cast<float>(MarkerIndex));
		const FVector Location(
			FMath::Cos(AngleRadians) * OrbitRadius,
			FMath::Sin(AngleRadians) * OrbitRadius,
			0.0f);
		MarkerMesh->UpdateInstanceTransform(
			MarkerIndex,
			FTransform(FRotator::ZeroRotator, Location,
				MarkerScale * GetVisualScaleMultiplier()),
			false,
			MarkerIndex == 2,
			true);
	}
}
