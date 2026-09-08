#include "Gameplay/AbilitySystem/GameplayCue/NPControlReversalGameplayCue.h"

#include "Components/InstancedStaticMeshComponent.h"

ANPControlReversalGameplayCue::ANPControlReversalGameplayCue()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
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

bool ANPControlReversalGameplayCue::WhileActive_Implementation(
	AActor* Target, const FGameplayCueParameters& Parameters)
{
	Super::WhileActive_Implementation(Target, Parameters);
	SceneRoot->SetWorldRotation(FRotator::ZeroRotator);
	OrbitAngle = 0.0f;
	RebuildGhostInstances();
	SetActorTickEnabled(true);
	return true;
}

bool ANPControlReversalGameplayCue::OnRemove_Implementation(
	AActor* Target, const FGameplayCueParameters& Parameters)
{
	SetActorTickEnabled(false);
	GhostMesh->ClearInstances();
	Super::OnRemove_Implementation(Target, Parameters);
	return true;
}

bool ANPControlReversalGameplayCue::Recycle()
{
	SetActorTickEnabled(false);
	OrbitAngle = 0.0f;
	if (GhostMesh)
	{
		GhostMesh->ClearInstances();
	}
	return Super::Recycle();
}

void ANPControlReversalGameplayCue::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	OrbitAngle = FMath::Fmod(
		OrbitAngle - FMath::DegreesToRadians(FMath::Max(0.0f, OrbitSpeed)) * DeltaSeconds,
		2.0f * PI);
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
			FTransform(Rotation, Location, GhostScale),
			false,
			Index == 3,
			true);
	}
}
