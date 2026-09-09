#include "Gameplay/AbilitySystem/GameplayCue/NPControlReversalGameplayCue.h"

#include "Components/InstancedStaticMeshComponent.h"

ANPControlReversalGameplayCue::ANPControlReversalGameplayCue()
{
	SceneRoot->SetAbsolute(false, true, true);

	GhostMesh = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("GhostMesh"));
	GhostMesh->SetupAttachment(SceneRoot);
	GhostMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GhostMesh->SetGenerateOverlapEvents(false);
	GhostMesh->SetCanEverAffectNavigation(false);
	GhostMesh->SetCastShadow(false);
}

void ANPControlReversalGameplayCue::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildGhostInstances();
}

void ANPControlReversalGameplayCue::PrepareVisual()
{
	SceneRoot->SetWorldRotation(FRotator::ZeroRotator);
	OrbitAngle = 0.0f;
	RebuildGhostInstances();
}

void ANPControlReversalGameplayCue::ResetVisual()
{
	OrbitAngle = 0.0f;
	if (GhostMesh)
	{
		GhostMesh->ClearInstances();
	}
}

void ANPControlReversalGameplayCue::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!IsActorTickEnabled())
	{
		return;
	}
	OrbitAngle = FMath::Fmod(
		OrbitAngle - FMath::DegreesToRadians(FMath::Max(0.0f, OrbitSpeed)) * DeltaSeconds,
		2.0f * PI);
	UpdateGhostTransforms();
}

void ANPControlReversalGameplayCue::ApplyVisualScale()
{
	UpdateGhostTransforms();
}

void ANPControlReversalGameplayCue::RebuildGhostInstances()
{
	GhostMesh->ClearInstances();
	for (int32 Index = 0; Index < 4; ++Index)
	{
		GhostMesh->AddInstance(FTransform::Identity);
	}
	UpdateGhostTransforms();
}

void ANPControlReversalGameplayCue::UpdateGhostTransforms()
{
	const float Radius = FMath::Max(1.0f, OrbitRadius);
	const float Amplitude = FMath::Max(0.0f, BobAmplitude);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const float Angle = OrbitAngle + Index * HALF_PI;
		const FVector Location(
			Radius * FMath::Cos(Angle),
			Radius * FMath::Sin(Angle),
			OrbitHeight + Amplitude * FMath::Sin(2.0f * Angle));
		const FVector Direction(
			Radius * FMath::Sin(Angle),
			-Radius * FMath::Cos(Angle),
			-2.0f * Amplitude * FMath::Cos(2.0f * Angle));
		const FQuat Rotation =
			Direction.Rotation().Quaternion() * GhostRotationOffset.Quaternion();
		GhostMesh->UpdateInstanceTransform(
			Index,
			FTransform(Rotation, Location, GhostScale * GetVisualScaleMultiplier()),
			false,
			Index == 3,
			true);
	}
}
