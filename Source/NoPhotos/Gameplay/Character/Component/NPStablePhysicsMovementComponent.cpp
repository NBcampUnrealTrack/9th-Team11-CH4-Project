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

void UNPStablePhysicsMovementComponent::SetClimbAcceleration(
	float InClimbAcceleration)
{
	ClimbAcceleration = FMath::Max(InClimbAcceleration, 0.0f);
}

void UNPStablePhysicsMovementComponent::SetGravityScale(float InGravityScale)
{
	GravityScale = FMath::Max(InGravityScale, 0.0f);
}

void UNPStablePhysicsMovementComponent::SetJumpVelocityChange(
	float InJumpVelocityChange)
{
	JumpVelocityChange = FMath::Max(InJumpVelocityChange, 0.0f);
}

void UNPStablePhysicsMovementComponent::SetJumpCooldown(float InJumpCooldown)
{
	JumpCooldown = FMath::Max(InJumpCooldown, 0.0f);
}

void UNPStablePhysicsMovementComponent::SetWalkableSlopeAngle(
	float InWalkableSlopeAngle)
{
	const float ClampedAngle = FMath::Clamp(InWalkableSlopeAngle, 0.0f, 90.0f);
	WalkableFloorZ = FMath::Cos(FMath::DegreesToRadians(ClampedAngle));
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

void UNPStablePhysicsMovementComponent::SetExternalFlowVelocity(
	UObject* Source,
	const FVector& InFlowVelocity)
{
	if (!IsValid(Source))
	{
		return;
	}

	FVector FlowVelocity = InFlowVelocity;
	FlowVelocity.Z = 0.0f;
	if (FlowVelocity.IsNearlyZero())
	{
		ExternalFlowVelocities.Remove(TWeakObjectPtr<UObject>(Source));
		return;
	}

	ExternalFlowVelocities.FindOrAdd(Source) = FlowVelocity;
}

void UNPStablePhysicsMovementComponent::ClearExternalFlowVelocity(UObject* Source)
{
	if (IsValid(Source))
	{
		ExternalFlowVelocities.Remove(TWeakObjectPtr<UObject>(Source));
	}
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
	bHasPelvisUprightReference = false;
	if (!PhysicsControl || !PhysicsMesh)
	{
		return;
	}

	const FBodyInstance* PelvisBody = PhysicsMesh->GetBodyInstance(PelvisBodyName);
	if (!PelvisBody || !PhysicsMesh->IsSimulatingPhysics(PelvisBodyName))
	{
		return;
	}

	const FQuat PelvisWorldRotation =
		PelvisBody->GetUnrealWorldTransform().GetRotation();
	const FVector VisualForward = GetCurrentFacingDirection();
	FacingTargetVisualYaw = VisualForward.Rotation().Yaw;
	const FQuat VisualYawRotation(
		FVector::UpVector,
		FMath::DegreesToRadians(FacingTargetVisualYaw));
	PelvisRotationFromVisualYaw =
		VisualYawRotation.Inverse() * PelvisWorldRotation;
	PelvisUprightLocalDirection = PelvisWorldRotation
		.UnrotateVector(FVector::UpVector)
		.GetSafeNormal();
	PelvisVisualForwardLocalDirection = PelvisWorldRotation
		.UnrotateVector(VisualForward)
		.GetSafeNormal();
	bHasPelvisUprightReference = true;

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
		&& !bTemporaryRagdollActive
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

void UNPStablePhysicsMovementComponent::SetTemporaryRagdollActive(
	const bool bActive)
{
	if (bTemporaryRagdollActive == bActive)
	{
		return;
	}

	bTemporaryRagdollActive = bActive;
	PendingInput.MoveInput = FVector::ZeroVector;
	PendingInput.bJumpRequested = false;
	if (bTemporaryRagdollActive)
	{
		SetFacingControlEnabled(false);
		return;
	}

	ResetFacingControlTarget();
}

void UNPStablePhysicsMovementComponent::SetTemporaryRagdollRecoveryActive(
	const bool bActive)
{
	bTemporaryRagdollRecoveryActive = bActive;
	if (bTemporaryRagdollRecoveryActive)
	{
		PendingInput.MoveInput = FVector::ZeroVector;
		PendingInput.bJumpRequested = false;
	}
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

FVector UNPStablePhysicsMovementComponent::GetExternalFlowVelocity()
{
	FVector TotalFlowVelocity = FVector::ZeroVector;
	int32 FlowCount = 0;
	for (auto Iterator = ExternalFlowVelocities.CreateIterator(); Iterator; ++Iterator)
	{
		if (!IsValid(Iterator.Key().Get()))
		{
			Iterator.RemoveCurrent();
			continue;
		}

		TotalFlowVelocity += Iterator.Value();
		++FlowCount;
	}

	return FlowCount > 0 ? TotalFlowVelocity / FlowCount : FVector::ZeroVector;
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
	RemainingJumpCooldown = FMath::Max(
		RemainingJumpCooldown - DeltaTime,
		0.0f);

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

	UpdateGravityPhysics();
	if (bTemporaryRagdollActive)
	{
		return;
	}

	UpdateGroundSupportPhysics();
	UpdateMovementPhysics(
		bTemporaryRagdollRecoveryActive
			? FVector::ZeroVector
			: Input.MoveInput);
	UpdateFacingPhysicsControl(
		DeltaTime,
		Input.FacingDirection,
		Input.bHasFacingDirection);
	if (!bTemporaryRagdollRecoveryActive)
	{
		UpdateRelicSwingRotation();
	}
	UpdateBalancePhysics();
	UpdateJumpPhysics(
		!bTemporaryRagdollRecoveryActive && Input.bJumpRequested);
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

	return Hit.ImpactNormal.Z >= WalkableFloorZ;
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
		QueryParams)
		|| Hit.ImpactNormal.Z < WalkableFloorZ)
	{
		return false;
	}

	OutGroundDistance = Hit.Distance + GroundProbeRadius;
	return true;
}

void UNPStablePhysicsMovementComponent::UpdateGravityPhysics()
{
	const float AdditionalGravityScale = GravityScale - 1.0f;
	if (FMath::IsNearlyZero(AdditionalGravityScale))
	{
		return;
	}

	const FVector AdditionalGravity(
		0.0f,
		0.0f,
		GetWorld()->GetGravityZ() * AdditionalGravityScale);
	PhysicsMesh->AddForceToAllBodiesBelow(
		AdditionalGravity,
		PelvisBodyName,
		true,
		true);
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
	FVector UpwardAcceleration = FVector::ZeroVector;
	if (InMoveInput.Z > UE_SMALL_NUMBER)
	{
		UpwardAcceleration = FVector::UpVector
			* ClimbAcceleration
			* FMath::Clamp(InMoveInput.Z, 0.0f, 1.0f);
		PhysicsMesh->AddForceToAllBodiesBelow(
			UpwardAcceleration,
			PelvisBodyName,
			true,
			true);
	}

	FVector HorizontalVelocity = Velocity;
	HorizontalVelocity.Z = 0.0f;

	// 힘을 계속 누적하지 않고 현재 속도가 목표 속도에 가까워지도록 제어합니다.
	const FVector DesiredVelocity = InMoveInput * MaxMoveSpeed + GetExternalFlowVelocity();
	FVector MoveForce = (DesiredVelocity - HorizontalVelocity) * MoveStrength;
	MoveForce -= HorizontalVelocity * MoveDamping;
	MoveForce.Z = 0.0f;

	const float ControlMultiplier = bGrounded ? 1.0f : AirControlMultiplier;
	MoveForce = (MoveForce * ControlMultiplier).GetClampedToMaxSize(MaxMoveForce);
	PhysicsMesh->AddForce(MoveForce, PelvisBodyName);

	const float TotalMass = FMath::Max(PhysicsMesh->GetMass(), 1.0f);
	CurrentAcceleration = MoveForce / TotalMass + UpwardAcceleration;
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

	const FQuat PelvisWorldRotation =
		PelvisBody->GetUnrealWorldTransform().GetRotation();
	if (bHasPelvisUprightReference)
	{
		FVector VisualForward = PelvisWorldRotation.RotateVector(
			PelvisVisualForwardLocalDirection);
		VisualForward.Z = 0.0f;
		if (!VisualForward.IsNearlyZero())
		{
			FacingTargetVisualYaw = VisualForward.Rotation().Yaw;
		}

		const FQuat VisualYawRotation(
			FVector::UpVector,
			FMath::DegreesToRadians(FacingTargetVisualYaw));
		FacingTargetOrientation =
			VisualYawRotation * PelvisRotationFromVisualYaw;
	}
	else
	{
		FRotator UprightTargetRotation = PelvisWorldRotation.Rotator();
		UprightTargetRotation.Pitch = 0.0f;
		UprightTargetRotation.Roll = 0.0f;
		FacingTargetOrientation = UprightTargetRotation.Quaternion();
	}
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
	const FBodyInstance* PelvisBody =
		PhysicsMesh->GetBodyInstance(PelvisBodyName);
	if (!PelvisBody)
	{
		return;
	}

	const FVector CurrentUp = PelvisBody->GetUnrealWorldTransform()
		.GetRotation()
		.RotateVector(PelvisUprightLocalDirection)
		.GetSafeNormal();
	const FVector TiltError = FVector::CrossProduct(CurrentUp, FVector::UpVector);
	FVector AngularVelocity = PhysicsMesh->GetPhysicsAngularVelocityInRadians(PelvisBodyName);
	AngularVelocity.Z = 0.0f;

	FVector BalanceTorque = TiltError * BalanceStrength - AngularVelocity * BalanceDamping;
	BalanceTorque.Z = 0.0f;
	BalanceTorque = BalanceTorque.GetClampedToMaxSize(MaxBalanceTorque);
	PhysicsMesh->AddTorqueInRadians(BalanceTorque, PelvisBodyName);
}

float UNPStablePhysicsMovementComponent::GetPelvisUprightDot() const
{
	if (!PhysicsMesh || !bHasPelvisUprightReference)
	{
		return -1.0f;
	}

	const FBodyInstance* PelvisBody =
		PhysicsMesh->GetBodyInstance(PelvisBodyName);
	if (!PelvisBody)
	{
		return -1.0f;
	}

	const FVector CurrentUp = PelvisBody->GetUnrealWorldTransform()
		.GetRotation()
		.RotateVector(PelvisUprightLocalDirection)
		.GetSafeNormal();
	return FVector::DotProduct(CurrentUp, FVector::UpVector);
}

void UNPStablePhysicsMovementComponent::UpdateJumpPhysics(bool bInJumpRequested)
{
	if (!bInJumpRequested || !bGrounded || RemainingJumpCooldown > 0.0f)
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
	RemainingJumpCooldown = JumpCooldown;
	OnJumpApplied.Broadcast();
}
