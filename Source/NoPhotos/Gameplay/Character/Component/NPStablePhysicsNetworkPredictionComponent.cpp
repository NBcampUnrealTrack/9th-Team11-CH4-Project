#include "Gameplay/Character/Component/NPStablePhysicsNetworkPredictionComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/HitResult.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Gameplay/Character/Component/NPStablePhysicsGrabComponent.h"
#include "Gameplay/Character/Component/NPStablePhysicsMovementComponent.h"
#include "Gameplay/Character/Component/NPControlReversalComponent.h"
#include "Gameplay/Photo/NPPhotoCapturePenaltyComponent.h"

UNPStablePhysicsNetworkPredictionComponent::UNPStablePhysicsNetworkPredictionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	SetIsReplicatedByDefault(true);
}

void UNPStablePhysicsNetworkPredictionComponent::Initialize(
	USkeletalMeshComponent* InPhysicsMesh,
	UNPStablePhysicsMovementComponent* InMovement,
	UNPStablePhysicsGrabComponent* InGrab,
	FName InRootBodyName)
{
	PhysicsMesh = InPhysicsMesh;
	Movement = InMovement;
	Grab = InGrab;
	RootBodyName = InRootBodyName;

	if (Movement)
	{
		AddTickPrerequisiteComponent(Movement);
	}
	if (Grab)
	{
		AddTickPrerequisiteComponent(Grab);
	}
}

void UNPStablePhysicsNetworkPredictionComponent::SetServerAuthoritativeInteraction(
	bool bActive)
{
	if (!GetOwner()
		|| !GetOwner()->HasAuthority()
		|| bServerAuthoritativeInteraction == bActive)
	{
		return;
	}

	bServerAuthoritativeInteraction = bActive;
	if (!bServerAuthoritativeInteraction)
	{
		bHasReceivedClientRootState = false;
	}
	ResetBlockedCorrectionTracking();
	GetOwner()->ForceNetUpdate();
}

void UNPStablePhysicsNetworkPredictionComponent::SendMoveInput(
	const FVector& WorldMoveInput, float InputViewYaw)
{
	if (!GetOwner() || GetOwner()->HasAuthority())
	{
		return;
	}

	if (WorldMoveInput.ContainsNaN() || !FMath::IsFinite(InputViewYaw))
	{
		SendStopMove();
		return;
	}
	PendingMoveInput = WorldMoveInput.GetClampedToMaxSize(1.0f);
	PendingMoveViewYaw = FRotator::NormalizeAxis(InputViewYaw);
}

void UNPStablePhysicsNetworkPredictionComponent::SendStopMove()
{
	if (!GetOwner() || GetOwner()->HasAuthority())
	{
		return;
	}

	++LocalInputSequence;
	PendingMoveInput = FVector::ZeroVector;
	PendingMoveViewYaw = 0.0f;
	InputSendAccumulator = 0.0f;
	ServerStopMove(LocalInputSequence);
}

void UNPStablePhysicsNetworkPredictionComponent::SendJumpRequest()
{
	if (!GetOwner() || GetOwner()->HasAuthority())
	{
		return;
	}

	ServerRequestJump();
}

void UNPStablePhysicsNetworkPredictionComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(
		UNPStablePhysicsNetworkPredictionComponent,
		ServerRootState,
		COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(
		UNPStablePhysicsNetworkPredictionComponent,
		bServerAuthoritativeInteraction,
		COND_OwnerOnly);
}

void UNPStablePhysicsNetworkPredictionComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!GetOwner() || !PhysicsMesh || RootBodyName.IsNone())
	{
		return;
	}

	if (GetOwner()->HasAuthority())
	{
		if (!bServerAuthoritativeInteraction)
		{
			ApplyServerCorrection(DeltaTime);
		}
		CaptureServerRootState();
		return;
	}

	if (GetOwner()->GetLocalRole() == ROLE_AutonomousProxy)
	{
		SendPendingMoveInput(DeltaTime);
		if (bServerAuthoritativeInteraction)
		{
			ApplyOwnerCorrection(DeltaTime);
		}
		else
		{
			SendClientRootState(DeltaTime);
		}
	}
}

void UNPStablePhysicsNetworkPredictionComponent::ServerSetMoveInput_Implementation(
	uint16 InputSequence,
	FVector_NetQuantizeNormal WorldMoveInput,
	float InputViewYaw)
{
	if (!Movement)
	{
		return;
	}

	const FVector ReceivedInput = FVector(WorldMoveInput);
	if (ReceivedInput.ContainsNaN() || !FMath::IsFinite(InputViewYaw) || !AcceptInputSequence(InputSequence))
	{
		return;
	}
	if (const UNPPhotoCapturePenaltyComponent* PhotoPenalty =
		GetOwner()->FindComponentByClass<UNPPhotoCapturePenaltyComponent>();
		PhotoPenalty && PhotoPenalty->IsPhotoStunActive())
	{
		if (UNPControlReversalComponent* Reversal =
			GetOwner()->FindComponentByClass<UNPControlReversalComponent>())
		{
			Reversal->ApplyRawMovementInput(FVector::ZeroVector, 0.0f);
		}
		else
		{
			Movement->SetMoveInput(FVector::ZeroVector);
		}
		if (Grab)
		{
			Grab->SetMovementIntent(FVector::ZeroVector);
		}
		return;
	}

	const FVector ClampedInput = ReceivedInput.GetClampedToMaxSize(1.0f);
	if (UNPControlReversalComponent* Reversal = GetOwner()->FindComponentByClass<UNPControlReversalComponent>())
	{
		Reversal->ApplyRawMovementInput(ClampedInput, InputViewYaw);
		return;
	}
	Movement->SetMoveInput(ClampedInput);
	if (Grab)
	{
		Grab->SetMovementIntent(ClampedInput);
	}
}

void UNPStablePhysicsNetworkPredictionComponent::ServerStopMove_Implementation(
	uint16 InputSequence)
{
	if (!Movement || !AcceptInputSequence(InputSequence))
	{
		return;
	}

	if (UNPControlReversalComponent* Reversal = GetOwner()->FindComponentByClass<UNPControlReversalComponent>())
	{
		// 정지 시 원본 캐시도 비워야 이후 태그 변경으로 이전 이동이 되살아나지 않습니다.
		Reversal->ApplyRawMovementInput(FVector::ZeroVector, 0.0f);
		return;
	}
	Movement->SetMoveInput(FVector::ZeroVector);
	if (Grab)
	{
		Grab->SetMovementIntent(FVector::ZeroVector);
	}
}

void UNPStablePhysicsNetworkPredictionComponent::ServerRequestJump_Implementation()
{
	if (!Movement)
	{
		return;
	}
	if (const UNPPhotoCapturePenaltyComponent* PhotoPenalty =
		GetOwner()->FindComponentByClass<UNPPhotoCapturePenaltyComponent>();
		PhotoPenalty && PhotoPenalty->IsPhotoStunActive())
	{
		return;
	}

	Movement->RequestJump();
}

void UNPStablePhysicsNetworkPredictionComponent::ServerSetClientRootState_Implementation(
	uint16 StateSequence,
	FNPStablePhysicsRootState NewRootState)
{
	if (bServerAuthoritativeInteraction
		|| FVector(NewRootState.Position).ContainsNaN()
		|| FVector(NewRootState.LinearVelocity).ContainsNaN()
		|| FVector(NewRootState.AngularVelocity).ContainsNaN()
		|| (bHasReceivedClientRootState
			&& static_cast<int16>(StateSequence - LastServerRootStateSequence) <= 0))
	{
		return;
	}

	bHasReceivedClientRootState = true;
	LastServerRootStateSequence = StateSequence;
	ClientRootState = NewRootState;
}

void UNPStablePhysicsNetworkPredictionComponent::OnRep_ServerRootState()
{
	bHasServerRootState = true;
}

void UNPStablePhysicsNetworkPredictionComponent::OnRep_ServerAuthoritativeInteraction()
{
	RecoveryCooldownRemaining = 0.0f;
	ResetBlockedCorrectionTracking();
}

void UNPStablePhysicsNetworkPredictionComponent::CaptureServerRootState()
{
	FBodyInstance* RootBody = PhysicsMesh->GetBodyInstance(RootBodyName);
	if (!RootBody)
	{
		return;
	}

	const FTransform RootTransform = RootBody->GetUnrealWorldTransform();
	ServerRootState.Position = RootTransform.GetLocation();
	ServerRootState.LinearVelocity = RootBody->GetUnrealWorldVelocity();
	ServerRootState.AngularVelocity = FMath::RadiansToDegrees(
		RootBody->GetUnrealWorldAngularVelocityInRadians());
	ServerRootState.ServerWorldTime = GetWorld()->GetTimeSeconds();
}

void UNPStablePhysicsNetworkPredictionComponent::ApplyOwnerCorrection(
	float DeltaTime)
{
	if (!bHasServerRootState)
	{
		return;
	}

	ApplyCorrection(
		ServerRootState,
		DeltaTime,
		bExternalGrabActive);
}

void UNPStablePhysicsNetworkPredictionComponent::ApplyServerCorrection(
	float DeltaTime)
{
	if (!bHasReceivedClientRootState)
	{
		return;
	}

	ApplyCorrection(ClientRootState, DeltaTime, false);
}

void UNPStablePhysicsNetworkPredictionComponent::ApplyCorrection(
	const FNPStablePhysicsRootState& TargetRootState,
	float DeltaTime,
	bool bUseFullBodyCorrection)
{
	if (DeltaTime <= UE_SMALL_NUMBER)
	{
		return;
	}

	FBodyInstance* RootBody = PhysicsMesh->GetBodyInstance(RootBodyName);
	if (!RootBody)
	{
		return;
	}

	RecoveryCooldownRemaining = FMath::Max(
		RecoveryCooldownRemaining - DeltaTime,
		0.0f);

	const float ServerStateAge =
		GetEstimatedServerWorldTime() - TargetRootState.ServerWorldTime;
	if (ServerStateAge > MaximumServerStateAge)
	{
		ResetBlockedCorrectionTracking();
		return;
	}

	const float ExtrapolationTime = FMath::Clamp(
		ServerStateAge,
		0.0f,
		MaximumExtrapolationTime);
	const FVector ExpectedServerPosition = FVector(TargetRootState.Position)
		+ FVector(TargetRootState.LinearVelocity) * ExtrapolationTime;
	const FVector CurrentRootPosition =
		RootBody->GetUnrealWorldTransform().GetLocation();
	const FVector PositionError =
		ExpectedServerPosition - CurrentRootPosition;
	const float PositionErrorSize = PositionError.Size();
	const bool bNearUnheldDynamicBody =
		IsNearUnheldDynamicBody(CurrentRootPosition);

	if (PositionErrorSize > HardSnapDistance
		&& IsRecoveryTargetClear(ExpectedServerPosition))
	{
		RecoverFullBody(ExpectedServerPosition, TargetRootState);
		return;
	}

	if (PositionErrorSize <= PositionErrorTolerance)
	{
		ResetBlockedCorrectionTracking();
		return;
	}

	if (RecoveryCooldownRemaining > 0.0f)
	{
		return;
	}

	if (!bNearUnheldDynamicBody
		&& ShouldRecoverFromBlockedCorrection(
			PositionErrorSize,
			PositionError,
			RootBody->GetUnrealWorldVelocity(),
			FVector(TargetRootState.LinearVelocity),
			DeltaTime)
		&& IsRecoveryTargetClear(ExpectedServerPosition))
	{
		RecoverFullBody(ExpectedServerPosition, TargetRootState);
		return;
	}
	if (bNearUnheldDynamicBody)
	{
		ResetBlockedCorrectionTracking();
	}

	const float SafeCorrectionTime = FMath::Max(PositionCorrectionTime, 0.01f);
	const FVector CorrectionVelocity = (PositionError / SafeCorrectionTime)
		.GetClampedToMaxSize(MaximumCorrectionSpeed);
	const FVector DesiredVelocity =
		FVector(TargetRootState.LinearVelocity) + CorrectionVelocity;
	FVector CorrectionAcceleration =
		(DesiredVelocity - RootBody->GetUnrealWorldVelocity())
		/ SafeCorrectionTime;
	CorrectionAcceleration = CorrectionAcceleration.GetClampedToMaxSize(
		MaximumCorrectionAcceleration);
	if (bNearUnheldDynamicBody)
	{
		CorrectionAcceleration *= DynamicBodyCorrectionScale;
	}

	FVector BlockingNormal = FVector::ZeroVector;
	const bool bBlockedByCorrectionSweep = FindBlockingCorrectionNormal(
		CurrentRootPosition,
		ExpectedServerPosition,
		BlockingNormal);
	if (bBlockedByCorrectionSweep)
	{
		const float IntoSurfaceAcceleration = FVector::DotProduct(
			CorrectionAcceleration,
			BlockingNormal);
		if (IntoSurfaceAcceleration < 0.0f)
		{
			CorrectionAcceleration -=
				BlockingNormal * IntoSurfaceAcceleration;
		}
	}
	if (bUseFullBodyCorrection)
	{
		PhysicsMesh->AddForceToAllBodiesBelow(
			CorrectionAcceleration,
			RootBodyName,
			true,
			true);
	}
	else
	{
		PhysicsMesh->AddForce(CorrectionAcceleration, RootBodyName, true);
	}
}

bool UNPStablePhysicsNetworkPredictionComponent::IsNearUnheldDynamicBody(
	const FVector& RootPosition) const
{
	if (!GetWorld() || DynamicBodyDetectionRadius <= UE_SMALL_NUMBER)
	{
		return false;
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(StablePhysicsNetworkCorrection),
		false,
		GetOwner());
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(
		Overlaps,
		RootPosition,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(DynamicBodyDetectionRadius),
		QueryParams);

	const UPrimitiveComponent* GrabbedComponent =
		Grab ? Grab->GetGrabbedComponent() : nullptr;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		const UPrimitiveComponent* OverlappedComponent = Overlap.GetComponent();
		if (OverlappedComponent
			&& OverlappedComponent != GrabbedComponent
			&& OverlappedComponent->IsSimulatingPhysics())
		{
			return true;
		}
	}

	return false;
}

bool UNPStablePhysicsNetworkPredictionComponent::FindBlockingCorrectionNormal(
	const FVector& Start,
	const FVector& End,
	FVector& OutBlockingNormal) const
{
	OutBlockingNormal = FVector::ZeroVector;
	if (!GetWorld()
		|| CorrectionSweepRadius <= UE_SMALL_NUMBER
		|| Start.Equals(End))
	{
		return false;
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(StablePhysicsBlockedCorrection),
		false,
		GetOwner());
	FHitResult Hit;
	if (!GetWorld()->SweepSingleByObjectType(
		Hit,
		Start,
		End,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(CorrectionSweepRadius),
		QueryParams))
	{
		return false;
	}

	OutBlockingNormal = Hit.ImpactNormal.GetSafeNormal();
	return !OutBlockingNormal.IsNearlyZero();
}

bool UNPStablePhysicsNetworkPredictionComponent::IsFullBodyCorrectionBlocked(
	const FVector& CorrectionDelta) const
{
	if (!GetWorld() || !PhysicsMesh || CorrectionDelta.IsNearlyZero())
	{
		return false;
	}

	FComponentQueryParams QueryParams(
		SCENE_QUERY_STAT(StablePhysicsFullBodyBlockedCorrection),
		GetOwner());
	if (Grab && Grab->GetGrabbedComponent())
	{
		QueryParams.AddIgnoredComponent(Grab->GetGrabbedComponent());
	}

	const FVector Start = PhysicsMesh->GetComponentLocation();
	TArray<FHitResult> Hits;
	GetWorld()->ComponentSweepMulti(
		Hits,
		PhysicsMesh,
		Start,
		Start + CorrectionDelta,
		PhysicsMesh->GetComponentQuat(),
		QueryParams);

	const FVector CorrectionDirection = CorrectionDelta.GetSafeNormal();
	const bool bDifferentVerticalLayer =
		FMath::Abs(CorrectionDelta.Z) >= VerticalLayerSeparationDistance;
	for (const FHitResult& Hit : Hits)
	{
		const UPrimitiveComponent* HitComponent = Hit.GetComponent();
		if (!Hit.bBlockingHit || Hit.bStartPenetrating || !HitComponent)
		{
			continue;
		}

		const ECollisionChannel ObjectType =
			HitComponent->GetCollisionObjectType();
		if (ObjectType != ECC_WorldStatic
			&& ObjectType != ECC_WorldDynamic)
		{
			continue;
		}

		const FVector BlockingNormal = Hit.ImpactNormal.GetSafeNormal();
		const bool bFloorOrCeiling = FMath::Abs(BlockingNormal.Z) >= 0.5f;
		if (bFloorOrCeiling && !bDifferentVerticalLayer)
		{
			continue;
		}

		if (FVector::DotProduct(CorrectionDirection, BlockingNormal) < -0.25f)
		{
			return true;
		}
	}

	return false;
}

bool UNPStablePhysicsNetworkPredictionComponent::IsRecoveryTargetClear(
	const FVector& TargetPosition) const
{
	if (!GetWorld() || CorrectionSweepRadius <= UE_SMALL_NUMBER)
	{
		return true;
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(StablePhysicsRecoveryTarget),
		false,
		GetOwner());
	const FVector CheckPosition =
		TargetPosition + FVector::UpVector * RecoveryTargetCheckHeightOffset;
	const bool bTargetBlocked = GetWorld()->OverlapAnyTestByObjectType(
		CheckPosition,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(CorrectionSweepRadius),
		QueryParams);

	if (bDrawRecoveryTargetDebug)
	{
		DrawDebugSphere(
			GetWorld(),
			CheckPosition,
			CorrectionSweepRadius,
			16,
			bTargetBlocked ? FColor::Red : FColor::Green,
			false,
			0.1f,
			0,
			2.0f);
	}

	return !bTargetBlocked;
}

bool UNPStablePhysicsNetworkPredictionComponent::ShouldRecoverFromBlockedCorrection(
	float PositionErrorSize,
	const FVector& PositionError,
	const FVector& CurrentVelocity,
	const FVector& TargetVelocity,
	float DeltaTime)
{
	const bool bHadPreviousError =
		PreviousCorrectionErrorSize > UE_SMALL_NUMBER;
	const float RequiredProgress =
		MinimumCorrectionProgressSpeed * DeltaTime;
	const bool bMadeProgress = !bHadPreviousError
		|| PreviousCorrectionErrorSize - PositionErrorSize >= RequiredProgress;
	const FVector ErrorDirection = PositionError.GetSafeNormal();
	const FVector RelativeVelocity =
		CurrentVelocity - TargetVelocity;
	const float ClosingSpeed = FVector::DotProduct(
		RelativeVelocity,
		ErrorDirection);
	const bool bClosingPositionError =
		ClosingSpeed >= MinimumCorrectionClosingSpeed;
	const bool bStableErrorDirection =
		PreviousCorrectionErrorDirection.IsNearlyZero()
		|| FVector::DotProduct(
			PreviousCorrectionErrorDirection,
			ErrorDirection) >= MinimumBlockedErrorDirectionDot;
	const bool bBlockedByWorld = PositionErrorSize >= BlockedCorrectionDistance
		&& !bMadeProgress
		&& !bClosingPositionError
		&& bStableErrorDirection
		&& IsFullBodyCorrectionBlocked(PositionError);

	if (bBlockedByWorld)
	{
		BlockedCorrectionTime += DeltaTime;
	}
	else
	{
		BlockedCorrectionTime = 0.0f;
	}

	PreviousCorrectionErrorSize = PositionErrorSize;
	PreviousCorrectionErrorDirection = ErrorDirection;
	return BlockedCorrectionTime >= BlockedCorrectionDelay;
}

void UNPStablePhysicsNetworkPredictionComponent::RecoverFullBody(
	const FVector& TargetPosition,
	const FNPStablePhysicsRootState& TargetRootState)
{
	PhysicsMesh->SetAllPhysicsPosition(TargetPosition);
	PhysicsMesh->SetAllPhysicsLinearVelocity(
		FVector(TargetRootState.LinearVelocity),
		false);
	PhysicsMesh->SetAllPhysicsAngularVelocityInRadians(
		FMath::DegreesToRadians(FVector(TargetRootState.AngularVelocity)),
		false);
	PhysicsMesh->WakeAllRigidBodies();
	RecoveryCooldownRemaining = RecoveryCooldown;
	ResetBlockedCorrectionTracking();
}

void UNPStablePhysicsNetworkPredictionComponent::ResetBlockedCorrectionTracking()
{
	PreviousCorrectionErrorSize = 0.0f;
	PreviousCorrectionErrorDirection = FVector::ZeroVector;
	BlockedCorrectionTime = 0.0f;
}

void UNPStablePhysicsNetworkPredictionComponent::SendPendingMoveInput(
	float DeltaTime)
{
	if (PendingMoveInput.IsNearlyZero())
	{
		return;
	}

	InputSendAccumulator += DeltaTime;
	const float SafeSendInterval = FMath::Max(InputSendInterval, 0.01f);
	if (InputSendAccumulator < SafeSendInterval)
	{
		return;
	}

	InputSendAccumulator = FMath::Fmod(InputSendAccumulator, SafeSendInterval);
	++LocalInputSequence;
	ServerSetMoveInput(LocalInputSequence, PendingMoveInput, PendingMoveViewYaw);
}

void UNPStablePhysicsNetworkPredictionComponent::SendClientRootState(
	float DeltaTime)
{
	RootStateSendAccumulator += DeltaTime;
	const float SafeSendInterval = FMath::Max(InputSendInterval, 0.01f);
	if (RootStateSendAccumulator < SafeSendInterval)
	{
		return;
	}

	FBodyInstance* RootBody = PhysicsMesh->GetBodyInstance(RootBodyName);
	if (!RootBody)
	{
		return;
	}

	RootStateSendAccumulator = FMath::Fmod(
		RootStateSendAccumulator,
		SafeSendInterval);
	const FTransform RootTransform = RootBody->GetUnrealWorldTransform();
	FNPStablePhysicsRootState NewRootState;
	NewRootState.Position = RootTransform.GetLocation();
	NewRootState.LinearVelocity = RootBody->GetUnrealWorldVelocity();
	NewRootState.AngularVelocity = FMath::RadiansToDegrees(
		RootBody->GetUnrealWorldAngularVelocityInRadians());
	NewRootState.ServerWorldTime = GetEstimatedServerWorldTime();
	ServerSetClientRootState(++LocalRootStateSequence, NewRootState);
}

bool UNPStablePhysicsNetworkPredictionComponent::AcceptInputSequence(
	uint16 InputSequence)
{
	if (bHasReceivedInput
		&& static_cast<int16>(InputSequence - LastServerInputSequence) <= 0)
	{
		return false;
	}

	bHasReceivedInput = true;
	LastServerInputSequence = InputSequence;
	return true;
}

float UNPStablePhysicsNetworkPredictionComponent::GetEstimatedServerWorldTime() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const AGameStateBase* GameState = World->GetGameState())
		{
			return GameState->GetServerWorldTimeSeconds();
		}
		return World->GetTimeSeconds();
	}

	return ServerRootState.ServerWorldTime;
}
