#include "Gameplay/Character/Component/NPControlReversalVisualComponent.h"

UNPControlReversalVisualComponent::UNPControlReversalVisualComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	SetCastShadow(false);
	SetAbsolute(false, true, true);
	SetHiddenInGame(true);
}

void UNPControlReversalVisualComponent::OnRegister()
{
	Super::OnRegister();
	ClearInstances();
	for (int32 Index = 0; Index < 4; ++Index)
	{
		AddInstance(FTransform::Identity);
	}
	UpdateGhostTransforms();
}

void UNPControlReversalVisualComponent::SetVisualActive(bool bActive)
{
	SetHiddenInGame(!bActive);
	SetComponentTickEnabled(bActive);
	if (bActive)
	{
		OrbitAngle = 0.0f;
		UpdateGhostTransforms();
	}
}

void UNPControlReversalVisualComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	// UE의 +X 전방 / +Y 우측 기준으로 위에서 보았을 때 반시계 방향입니다.
	OrbitAngle = FMath::Fmod(OrbitAngle - FMath::DegreesToRadians(FMath::Max(0.0f, OrbitSpeed)) * DeltaTime,
		2.0f * PI);
	UpdateGhostTransforms();
}

void UNPControlReversalVisualComponent::UpdateGhostTransforms()
{
	const float Radius = FMath::Max(1.0f, OrbitRadius);
	const float Amplitude = FMath::Max(0.0f, BobAmplitude);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const float Angle = OrbitAngle + Index * HALF_PI;
		const FVector Location(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle),
			OrbitHeight + Amplitude * FMath::Sin(2.0f * Angle));
		// 높낮이까지 포함한 궤도의 접선 방향으로 정면을 맞춥니다.
		const FVector Direction(Radius * FMath::Sin(Angle), -Radius * FMath::Cos(Angle),
			-2.0f * Amplitude * FMath::Cos(2.0f * Angle));
		const FQuat Rotation = Direction.Rotation().Quaternion() * GhostRotationOffset.Quaternion();
		UpdateInstanceTransform(Index, FTransform(Rotation, Location, GhostScale), false, Index == 3, true);
	}
}
