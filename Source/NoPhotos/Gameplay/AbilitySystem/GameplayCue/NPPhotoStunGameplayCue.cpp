#include "Gameplay/AbilitySystem/GameplayCue/NPPhotoStunGameplayCue.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Gameplay/Character/NPReplicatedStablePhysicsPawn.h"
#include "Gameplay/Character/NPStatusVisualManager.h"
#include "UObject/ConstructorHelpers.h"

ANPPhotoStunGameplayCue::ANPPhotoStunGameplayCue()
{
	bAutoDestroyOnRemove = false;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
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

bool ANPPhotoStunGameplayCue::WhileActive_Implementation(
	AActor* Target, const FGameplayCueParameters& Parameters)
{
	Super::WhileActive_Implementation(Target, Parameters);
	SceneRoot->SetWorldRotation(FRotator::ZeroRotator);
	MarkerMesh->SetRelativeRotation(FRotator::ZeroRotator);
	ScaleMultiplier = 0.0f;
	RebuildMarkerInstances();
	SetActorTickEnabled(true);
	if (const ANPReplicatedStablePhysicsPawn* Pawn = Cast<ANPReplicatedStablePhysicsPawn>(Target))
	{
		if (ANPStatusVisualManager* Manager = Pawn->GetStatusVisualManager())
		{
			Manager->RequestPhotoStunVisual(this, true);
			return true;
		}
	}
	SetManagedScaleMultiplier(1.0f);
	return true;
}

bool ANPPhotoStunGameplayCue::OnRemove_Implementation(
	AActor* Target, const FGameplayCueParameters& Parameters)
{
	if (!Target)
	{
		MarkerMesh->ClearInstances();
		Super::OnRemove_Implementation(Target, Parameters);
		return true;
	}
	if (const ANPReplicatedStablePhysicsPawn* Pawn = Cast<ANPReplicatedStablePhysicsPawn>(Target))
	{
		if (ANPStatusVisualManager* Manager = Pawn->GetStatusVisualManager())
		{
			Manager->RequestPhotoStunVisual(this, false);
			return true;
		}
	}
	CompleteManagedRemoval();
	return true;
}

bool ANPPhotoStunGameplayCue::Recycle()
{
	SetActorTickEnabled(false);
	ScaleMultiplier = 0.0f;
	if (MarkerMesh)
	{
		MarkerMesh->SetRelativeRotation(FRotator::ZeroRotator);
		MarkerMesh->ClearInstances();
	}
	return Super::Recycle();
}

void ANPPhotoStunGameplayCue::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	MarkerMesh->AddLocalRotation(FRotator(
		0.0f,
		RotationSpeedDegrees * FMath::Max(DeltaSeconds, 0.0f),
		0.0f));
}

void ANPPhotoStunGameplayCue::SetManagedScaleMultiplier(float NewScaleMultiplier)
{
	ScaleMultiplier = NewScaleMultiplier;
	UpdateMarkerInstances();
}

void ANPPhotoStunGameplayCue::CompleteManagedRemoval()
{
	SetActorTickEnabled(false);
	MarkerMesh->ClearInstances();
	GameplayCueFinishedCallback();
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
			FTransform(FRotator::ZeroRotator, Location, MarkerScale * ScaleMultiplier));
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
			FTransform(FRotator::ZeroRotator, Location, MarkerScale * ScaleMultiplier),
			false,
			MarkerIndex == 2,
			true);
	}
}
