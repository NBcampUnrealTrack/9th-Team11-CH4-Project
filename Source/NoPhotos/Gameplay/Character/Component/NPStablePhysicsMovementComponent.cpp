#include "Gameplay/Character/Component/NPStablePhysicsMovementComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "PhysicsControlComponent.h"
#include "PhysicsControlData.h"
#include "PhysicsEngine/BodyInstance.h"

UNPStablePhysicsMovementComponent::UNPStablePhysicsMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UNPStablePhysicsMovementComponent::Initialize(
	USkeletalMeshComponent* InPhysicsMesh,
	float InCharacterForwardYawOffset)
{
	PhysicsMesh = InPhysicsMesh;
	CharacterForwardYawOffset = InCharacterForwardYawOffset;
}

void UNPStablePhysicsMovementComponent::ConfigureBoneNames(
	FName InPelvisBodyName,
	FName InLeftFootBoneName,
	FName InRightFootBoneName)
{
	PelvisBodyName = InPelvisBodyName;
	LeftFootBoneName = InLeftFootBoneName;
	RightFootBoneName = InRightFootBoneName;
}

void UNPStablePhysicsMovementComponent::SetTargetPelvisHeight(
	float InTargetPelvisHeight)
{
	TargetPelvisHeight = FMath::Max(InTargetPelvisHeight, 0.0f);
}

void UNPStablePhysicsMovementComponent::SetMaxMoveSpeed(float InMaxMoveSpeed)
{
	MaxMoveSpeed = FMath::Max(InMaxMoveSpeed, 0.0f);
}

void UNPStablePhysicsMovementComponent::SetJumpVelocityChange(
	float InJumpVelocityChange)
{
	JumpVelocityChange = FMath::Max(InJumpVelocityChange, 0.0f);
}

void UNPStablePhysicsMovementComponent::SetFacingControlSettings(
	float InAngularStrength,
	float InAngularDampingRatio,
	float InMaxTorque,
	float InMaxTargetSpeed)
{
	FacingAngularStrength = FMath::Max(InAngularStrength, 0.0f);
	FacingAngularDampingRatio = FMath::Max(InAngularDampingRatio, 0.0f);
	MaxFacingTorque = FMath::Max(InMaxTorque, 0.0f);
	MaxFacingTargetSpeed = FMath::Max(InMaxTargetSpeed, 0.0f);
}

void UNPStablePhysicsMovementComponent::SetMoveInput(const FVector& InMoveInput)
{
	PendingInput.MoveInput = InMoveInput.GetClampedToMaxSize(1.0f);
}

void UNPStablePhysicsMovementComponent::SetFacingDirection(
	const FVector& InFacingDirection)
{
	FVector HorizontalDirection(InFacingDirection.X, InFacingDirection.Y, 0.0f);
	if (!HorizontalDirection.IsNearlyZero())
	{
		PendingInput.FacingDirection = HorizontalDirection.GetSafeNormal();
		PendingInput.bHasFacingDirection = true;
	}
}

void UNPStablePhysicsMovementComponent::InitializeFacingControl(
	UPhysicsControlComponent* InPhysicsControl)
{
	if (PhysicsControl && bFacingControlCreated)
	{
		PhysicsControl->DestroyControl(FacingControlName, true, false);
	}

	PhysicsControl = InPhysicsControl;
	bFacingControlCreated = false;
	bFacingControlEnabled = false;
	if (!PhysicsControl || !PhysicsMesh)
	{
		return;
	}

	const FBodyInstance* PelvisBody = PhysicsMesh->GetBodyInstance(PelvisBodyName);
	if (!PelvisBody || !PhysicsMesh->IsSimulatingPhysics(PelvisBodyName))
	{
		return;
	}

	FPhysicsControlData ControlData;
	ControlData.bEnabled = false;
	ControlData.LinearStrength = 0.0f;
	ControlData.LinearDampingRatio = 0.0f;
	ControlData.LinearExtraDamping = 0.0f;
	ControlData.MaxForce = 0.0f;
	ControlData.AngularStrength = FacingAngularStrength;
	ControlData.AngularDampingRatio = FacingAngularDampingRatio;
	ControlData.MaxTorque = MaxFacingTorque;
	ControlData.bUseSkeletalAnimation = false;
	ControlData.bOnlyControlChildObject = true;

	FPhysicsControlTarget ControlTarget;
	ControlTarget.TargetOrientation = PelvisBody->GetUnrealWorldTransform().Rotator();
	ControlTarget.bApplyControlPointToTarget = true;

	bFacingControlCreated = PhysicsControl->CreateNamedControl(
		FacingControlName,
		nullptr,
		NAME_None,
		PhysicsMesh,
		PelvisBodyName,
		ControlData,
		ControlTarget,
		NAME_None);
	ResetFacingControlTarget();
}

void UNPStablePhysicsMovementComponent::SetFacingControlEnabled(bool bEnabled)
{
	const bool bShouldEnable = bEnabled
		&& !bFacingControlSuppressed
		&& bOrientRotationToMovement
		&& bFacingControlCreated;
	if (!PhysicsControl || bFacingControlEnabled == bShouldEnable)
	{
		return;
	}

	if (bShouldEnable)
	{
		ResetFacingControlTarget();
	}
	PhysicsControl->SetControlEnabled(
		FacingControlName,
		bShouldEnable,
		true,
		false);
	bFacingControlEnabled = bShouldEnable;
}

void UNPStablePhysicsMovementComponent::BeginRelicSwingRotation(
	float Torque,
	float MaxAngularSpeedDegrees)
{
	RelicSwingTorque = Torque;
	MaxRelicSwingAngularSpeed = FMath::DegreesToRadians(
		FMath::Max(MaxAngularSpeedDegrees, 0.0f));
	bRelicSwingRotationActive = !FMath::IsNearlyZero(RelicSwingTorque);
	bFacingControlSuppressed = true;
	SetFacingControlEnabled(false);
}

void UNPStablePhysicsMovementComponent::EndRelicSwingRotation()
{
	bRelicSwingRotationActive = false;
	RelicSwingTorque = 0.0f;
	MaxRelicSwingAngularSpeed = 0.0f;
	bFacingControlSuppressed = false;
	ResetFacingControlTarget();
}

FVector UNPStablePhysicsMovementComponent::GetCurrentFacingDirection() const
{
	if (!PhysicsMesh)
	{
		return FVector::ForwardVector;
	}

	FVector CurrentForward = PhysicsMesh->GetForwardVector().RotateAngleAxis(
		CharacterForwardYawOffset,
		FVector::UpVector);
	CurrentForward.Z = 0.0f;
	return CurrentForward.GetSafeNormal();
}

void UNPStablePhysicsMovementComponent::SetAnimationStateOverride(
	bool bEnabled,
	const FVector& InVelocity,
	const FVector& InAcceleration,
	bool bInIsFalling)
{
	bUseAnimationStateOverride = bEnabled;
	AnimationVelocity = InVelocity;
	AnimationAcceleration = InAcceleration;
	bAnimationIsFalling = bInIsFalling;
}

void UNPStablePhysicsMovementComponent::RequestJump()
{
	PendingInput.bJumpRequested = true;
}

void UNPStablePhysicsMovementComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!PhysicsMesh || !PhysicsMesh->IsSimulatingPhysics(PelvisBodyName))
	{
		Velocity = FVector::ZeroVector;
		CurrentAcceleration = FVector::ZeroVector;
		bGrounded = false;
		bIsFalling = true;
		ConsumePendingInput();
		return;
	}

	const FNPStablePhysicsLocomotionInput Input = ConsumePendingInput();
	SimulateLocomotion(DeltaTime, Input);
}

FNPStablePhysicsLocomotionInput UNPStablePhysicsMovementComponent::ConsumePendingInput()
{
	const FNPStablePhysicsLocomotionInput Input = PendingInput;
	PendingInput.bJumpRequested = false;
	return Input;
}

void UNPStablePhysicsMovementComponent::SimulateLocomotion(
	float DeltaTime,
	const FNPStablePhysicsLocomotionInput& Input)
{
	UpdateMovementState();
	UpdateGroundedState();
	if (!bPhysicsUpdatesEnabled)
	{
		return;
	}

	UpdateGroundSupportPhysics();
	UpdateMovementPhysics(Input.MoveInput);
	UpdateFacingPhysicsControl(
		DeltaTime,
		Input.FacingDirection,
		Input.bHasFacingDirection);
	UpdateRelicSwingRotation();
	UpdateBalancePhysics();
	UpdateJumpPhysics(Input.bJumpRequested);
}

void UNPStablePhysicsMovementComponent::UpdateMovementState()
{
	Velocity = PhysicsMesh->GetPhysicsLinearVelocity(PelvisBodyName);
	CurrentAcceleration = FVector::ZeroVector;
}

void UNPStablePhysicsMovementComponent::UpdateGroundedState()
{
	bGrounded = IsFootGrounded(LeftFootBoneName) || IsFootGrounded(RightFootBoneName);
	bIsFalling = !bGrounded;
}

bool UNPStablePhysicsMovementComponent::IsFootGrounded(FName FootBoneName) const
{
	if (PhysicsMesh->GetBoneIndex(FootBoneName) == INDEX_NONE)
	{
		return false;
	}

	const FVector Start = PhysicsMesh->GetSocketLocation(FootBoneName) + FVector::UpVector * GroundProbeRadius;
	const FVector End = Start - FVector::UpVector * (GroundProbeRadius * 2.0f + 20.0f);

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(StablePhysicsGround), false, GetOwner());
	FHitResult Hit;
	return GetWorld()->SweepSingleByObjectType(
		Hit,
		Start,
		End,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(GroundProbeRadius),
		QueryParams);
}

bool UNPStablePhysicsMovementComponent::FindPelvisGroundDistance(float& OutGroundDistance) const
{
	const FVector Start = PhysicsMesh->GetSocketLocation(PelvisBodyName);
	const FVector End = Start - FVector::UpVector * GroundTraceDistance;

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(StablePhysicsSupport), false, GetOwner());
	FHitResult Hit;
	if (!GetWorld()->SweepSingleByObjectType(
		Hit,
		Start,
		End,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(GroundProbeRadius),
		QueryParams))
	{
		return false;
	}

	OutGroundDistance = Hit.Distance + GroundProbeRadius;
	return true;
}

void UNPStablePhysicsMovementComponent::UpdateGroundSupportPhysics()
{
	float GroundDistance = 0.0f;
	if (!FindPelvisGroundDistance(GroundDistance))
	{
		return;
	}

	const float HeightError = TargetPelvisHeight - GroundDistance;
	if (HeightError <= 0.0f)
	{
		return;
	}

	const float SupportAcceleration = FMath::Clamp(
		HeightError * SupportStrength - Velocity.Z * SupportDamping,
		0.0f,
		MaxSupportAcceleration);
	const float TotalMass = FMath::Max(PhysicsMesh->GetMass(), 1.0f);
	PhysicsMesh->AddForce(FVector::UpVector * SupportAcceleration * TotalMass, PelvisBodyName);
}

void UNPStablePhysicsMovementComponent::UpdateMovementPhysics(const FVector& InMoveInput)
{
	FVector HorizontalVelocity = Velocity;
	HorizontalVelocity.Z = 0.0f;

	// 힘을 계속 누적하지 않고 현재 속도가 목표 속도에 가까워지도록 제어합니다.
	const FVector DesiredVelocity = InMoveInput * MaxMoveSpeed;
	FVector MoveForce = (DesiredVelocity - HorizontalVelocity) * MoveStrength;
	MoveForce -= HorizontalVelocity * MoveDamping;
	MoveForce.Z = 0.0f;

	const float ControlMultiplier = bGrounded ? 1.0f : AirControlMultiplier;
	MoveForce = (MoveForce * ControlMultiplier).GetClampedToMaxSize(MaxMoveForce);
	PhysicsMesh->AddForce(MoveForce, PelvisBodyName);

	const float TotalMass = FMath::Max(PhysicsMesh->GetMass(), 1.0f);
	CurrentAcceleration = MoveForce / TotalMass;
}

void UNPStablePhysicsMovementComponent::UpdateFacingPhysicsControl(
	float DeltaTime,
	const FVector& InFacingDirection,
	bool bInHasFacingDirection)
{
	if (!bFacingControlEnabled
		|| !bInHasFacingDirection
		|| DeltaTime <= UE_SMALL_NUMBER)
	{
		return;
	}

	const float DesiredVisualYaw = InFacingDirection.Rotation().Yaw;
	const float PreviousTargetYaw = FacingTargetVisualYaw;
	FacingTargetVisualYaw = FMath::FixedTurn(
		FacingTargetVisualYaw,
		DesiredVisualYaw,
		MaxFacingTargetSpeed * DeltaTime);
	const float TargetYawDelta = FMath::FindDeltaAngleDegrees(
		PreviousTargetYaw,
		FacingTargetVisualYaw);
	FacingTargetOrientation = FQuat(
		FVector::UpVector,
		FMath::DegreesToRadians(TargetYawDelta)) * FacingTargetOrientation;

	PhysicsControl->SetControlTargetOrientation(
		FacingControlName,
		FacingTargetOrientation.Rotator(),
		DeltaTime,
		true,
		true,
		true,
		false);
}

void UNPStablePhysicsMovementComponent::ResetFacingControlTarget()
{
	if (!PhysicsMesh)
	{
		return;
	}

	const FBodyInstance* PelvisBody = PhysicsMesh->GetBodyInstance(PelvisBodyName);
	if (!PelvisBody)
	{
		return;
	}

	FacingTargetOrientation = PelvisBody->GetUnrealWorldTransform().GetRotation();
	FacingTargetVisualYaw = GetCurrentFacingDirection().Rotation().Yaw;
	if (PhysicsControl && bFacingControlCreated)
	{
		PhysicsControl->SetControlTargetOrientation(
			FacingControlName,
			FacingTargetOrientation.Rotator(),
			0.0f,
			false,
			true,
			true,
			false);
	}
}

void UNPStablePhysicsMovementComponent::UpdateRelicSwingRotation()
{
	if (!bRelicSwingRotationActive || !PhysicsMesh)
	{
		return;
	}

	const float TorqueDirection = FMath::Sign(RelicSwingTorque);
	const float CurrentAngularSpeed =
		PhysicsMesh->GetPhysicsAngularVelocityInRadians(PelvisBodyName).Z;
	const float DirectedAngularSpeed = CurrentAngularSpeed * TorqueDirection;
	if (MaxRelicSwingAngularSpeed > UE_SMALL_NUMBER
		&& DirectedAngularSpeed >= MaxRelicSwingAngularSpeed)
	{
		return;
	}

	PhysicsMesh->AddTorqueInRadians(
		FVector::UpVector * RelicSwingTorque,
		PelvisBodyName);
}

void UNPStablePhysicsMovementComponent::UpdateBalancePhysics()
{
	const FVector CurrentUp = PhysicsMesh->GetUpVector();
	const FVector TiltError = FVector::CrossProduct(CurrentUp, FVector::UpVector);
	FVector AngularVelocity = PhysicsMesh->GetPhysicsAngularVelocityInRadians(PelvisBodyName);
	AngularVelocity.Z = 0.0f;

	FVector BalanceTorque = TiltError * BalanceStrength - AngularVelocity * BalanceDamping;
	BalanceTorque.Z = 0.0f;
	BalanceTorque = BalanceTorque.GetClampedToMaxSize(MaxBalanceTorque);
	PhysicsMesh->AddTorqueInRadians(BalanceTorque, PelvisBodyName);
}

void UNPStablePhysicsMovementComponent::UpdateJumpPhysics(bool bInJumpRequested)
{
	if (!bInJumpRequested || !bGrounded)
	{
		return;
	}

	PhysicsMesh->AddImpulseToAllBodiesBelow(
		FVector::UpVector * JumpVelocityChange,
		PelvisBodyName,
		true,
		true);
	bGrounded = false;
	bIsFalling = true;
	OnJumpApplied.Broadcast();
}
