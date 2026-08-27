#include "Gameplay/Character/NPReplicatedStablePhysicsPawn.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Gameplay/AbilitySystem/NPAbilitySystemComponent.h"
#include "Gameplay/Character/Component/NPStablePhysicsGrabComponent.h"
#include "Gameplay/Character/Component/NPStablePhysicsNetworkPredictionComponent.h"
#include "Gameplay/Interaction/Components/GrabbableComponent.h"
#include "Gameplay/Relic/NPBaseRelic.h"
#include "Core/NPPlayerState.h"
#include "Gameplay/Character/Component/NPStablePhysicsMovementComponent.h"

ANPReplicatedStablePhysicsPawn::ANPReplicatedStablePhysicsPawn()
{
	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(30.0f);

	// 소유 클라이언트는 공유 Grab 중에만 서버 Root 상태를 별도로 보정받습니다.
	PhysicsMesh->bReplicatePhysicsToAutonomousProxy = false;

	NetworkPrediction = CreateDefaultSubobject<
		UNPStablePhysicsNetworkPredictionComponent>(TEXT("NetworkPrediction"));

	AbilitySystem = CreateDefaultSubobject<UNPAbilitySystemComponent>(
		TEXT("AbilitySystem"));
}

void ANPReplicatedStablePhysicsPawn::BeginPlay()
{
	Super::BeginPlay();
	AbilitySystem->InitializeForOwner();

	const bool bServerAuthority = HasAuthority();
	const bool bRunsMovementPhysics = bServerAuthority || IsLocallyControlled();
	PhysicsMovement->SetPhysicsUpdatesEnabled(bRunsMovementPhysics);
	RightHandGrab->SetGrabSimulationEnabled(
		bServerAuthority || IsLocallyControlled());
	NetworkPrediction->Initialize(
		PhysicsMesh,
		PhysicsMovement,
		RightHandGrab,
		FullBodyRootName);
	NetworkPrediction->SetExternalGrabActive(bExternallyGrabbed);
	if (!bRunsMovementPhysics)
	{
		PhysicsMovement->SetAnimationStateOverride(
			true,
			FVector(ReplicatedAnimationVelocity),
			FVector(ReplicatedAnimationAcceleration),
			bReplicatedAnimationIsFalling);
	}

	if (bServerAuthority)
	{
		RightHandGrab->OnGrabbedComponentChanged.AddUObject(
			this,
			&ANPReplicatedStablePhysicsPawn::HandleGrabbedComponentChanged);
	}
	else if (IsReplicatedGrabActive())
	{
		OnRep_GrabState();
	}
}

void ANPReplicatedStablePhysicsPawn::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	AbilitySystem->InitializeForOwner();
}

void ANPReplicatedStablePhysicsPawn::OnRep_Controller()
{
	Super::OnRep_Controller();
	AbilitySystem->InitializeForOwner();
}

void ANPReplicatedStablePhysicsPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority() && IsValid(ExternallyGrabbedTargetPawn))
	{
		ANPReplicatedStablePhysicsPawn* TargetPawn = ExternallyGrabbedTargetPawn;
		ExternallyGrabbedTargetPawn = nullptr;
		TargetPawn->RemoveExternalGrabber();
	}

	Super::EndPlay(EndPlayReason);
}

void ANPReplicatedStablePhysicsPawn::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ANPReplicatedStablePhysicsPawn, bReplicatedRightHandActive);
	DOREPLIFETIME(ANPReplicatedStablePhysicsPawn, ReplicatedGrabState);
	DOREPLIFETIME_CONDITION(
		ANPReplicatedStablePhysicsPawn,
		bExternallyGrabbed,
		COND_OwnerOnly);
	DOREPLIFETIME(ANPReplicatedStablePhysicsPawn, ReplicatedServerHandWorldLocation);
	DOREPLIFETIME(ANPReplicatedStablePhysicsPawn, ReplicatedAnimationVelocity);
	DOREPLIFETIME(ANPReplicatedStablePhysicsPawn, ReplicatedAnimationAcceleration);
	DOREPLIFETIME(ANPReplicatedStablePhysicsPawn, bReplicatedAnimationIsFalling);
	DOREPLIFETIME(ANPReplicatedStablePhysicsPawn, ReplicatedAnimationForwardDirection);
	DOREPLIFETIME_CONDITION(
		ANPReplicatedStablePhysicsPawn,
		ReplicatedViewRotation,
		COND_SkipOwner);
}

UAbilitySystemComponent* ANPReplicatedStablePhysicsPawn::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void ANPReplicatedStablePhysicsPawn::AddExternalVelocityChange(
	const FVector& VelocityChange)
{
	if (!HasAuthority() || VelocityChange.IsNearlyZero())
	{
		return;
	}

	ApplyExternalVelocityChangeLocal(VelocityChange);
	if (IsPlayerControlled() && !IsLocallyControlled())
	{
		ClientApplyExternalVelocityChange(VelocityChange);
	}
}

void ANPReplicatedStablePhysicsPawn::ClientApplyExternalVelocityChange_Implementation(
	FVector_NetQuantize10 VelocityChange)
{
	ApplyExternalVelocityChangeLocal(FVector(VelocityChange));
}

void ANPReplicatedStablePhysicsPawn::SetupPlayerInputComponent(
	UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInputComponent =
		Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (EnhancedInputComponent && RelicUseAction)
	{
		AbilitySystem->BindRelicUseInput(
			EnhancedInputComponent,
			RelicUseAction);
	}
}

void ANPReplicatedStablePhysicsPawn::Tick(float DeltaSeconds)
{
	UpdateViewRotationReplication(DeltaSeconds);
	UpdateClientSimulationState();
	UpdateReplicatedGrabVisualTarget();
	UpdateLocalPredictedGrab(DeltaSeconds);

	Super::Tick(DeltaSeconds);
	if (HasAuthority())
	{
		PhysicsMovement->SetFacingControlEnabled(true);
		UpdateServerReplicatedState();
	}
}

void ANPReplicatedStablePhysicsPawn::UpdateClientSimulationState()
{
	if (HasAuthority())
	{
		return;
	}

	const bool bRunsMovementPhysics = IsLocallyControlled();
	PhysicsMovement->SetPhysicsUpdatesEnabled(bRunsMovementPhysics);
	if (bRunsMovementPhysics)
	{
		PhysicsMovement->SetAnimationStateOverride(
			false,
			FVector::ZeroVector,
			FVector::ZeroVector,
			true);
		return;
	}

	FVector ServerForward = FVector(ReplicatedAnimationForwardDirection);
	ServerForward.Z = 0.0f;
	ServerForward.Normalize();

	FVector ClientForward = GetActorForwardVector();
	ClientForward.Z = 0.0f;
	ClientForward.Normalize();

	const FVector ServerRight = FVector::CrossProduct(
		FVector::UpVector,
		ServerForward);
	const FVector ClientRight = FVector::CrossProduct(
		FVector::UpVector,
		ClientForward);
	const FVector ServerVelocity = FVector(ReplicatedAnimationVelocity);
	const FVector ServerAcceleration = FVector(ReplicatedAnimationAcceleration);
	const FVector ClientAnimationVelocity =
		ClientForward * FVector::DotProduct(ServerVelocity, ServerForward)
		+ ClientRight * FVector::DotProduct(ServerVelocity, ServerRight)
		+ FVector::UpVector * ServerVelocity.Z;
	const FVector ClientAnimationAcceleration =
		ClientForward * FVector::DotProduct(ServerAcceleration, ServerForward)
		+ ClientRight * FVector::DotProduct(ServerAcceleration, ServerRight)
		+ FVector::UpVector * ServerAcceleration.Z;

	PhysicsMovement->SetAnimationStateOverride(
		true,
		ClientAnimationVelocity,
		ClientAnimationAcceleration,
		bReplicatedAnimationIsFalling);
}

void ANPReplicatedStablePhysicsPawn::UpdateServerReplicatedState()
{
	ReplicatedAnimationVelocity = PhysicsMovement->GetVelocity();
	ReplicatedAnimationAcceleration = PhysicsMovement->GetCurrentAcceleration();
	bReplicatedAnimationIsFalling = PhysicsMovement->GetIsFalling();

	FVector AnimationForward = GetActorForwardVector();
	AnimationForward.Z = 0.0f;
	ReplicatedAnimationForwardDirection = AnimationForward.GetSafeNormal();

	if (IsReplicatedGrabActive()
		&& PhysicsMesh->GetBoneIndex(RightHandBoneName) != INDEX_NONE)
	{
		ReplicatedServerHandWorldLocation =
			PhysicsMesh->GetSocketLocation(RightHandBoneName);
	}

	bool bSharedRelicGrab = false;
	if (const ANPBaseRelic* HeldRelic =
		Cast<ANPBaseRelic>(ReplicatedGrabState.GrabbedActor))
	{
		if (const UGrabbableComponent* Grabbable =
			HeldRelic->FindComponentByClass<UGrabbableComponent>())
		{
			bSharedRelicGrab = Grabbable->GetActiveGrabCount() > 1;
		}
	}

	NetworkPrediction->SetServerAuthoritativeInteraction(
		bExternallyGrabbed
		|| IsValid(ExternallyGrabbedTargetPawn)
		|| bSharedRelicGrab);
}

void ANPReplicatedStablePhysicsPawn::ApplyMoveInput(const FVector& WorldMoveInput)
{
	const FVector ClampedMoveInput = WorldMoveInput.GetClampedToMaxSize(1.0f);
	if (HasAuthority())
	{
		Super::ApplyMoveInput(ClampedMoveInput);
		return;
	}

	if (!IsLocallyControlled())
	{
		return;
	}

	// 소유 클라이언트에서도 즉시 물리를 적용하고 동일 입력을 서버에 전달합니다.
	Super::ApplyMoveInput(ClampedMoveInput);

	if (ClampedMoveInput.IsNearlyZero())
	{
		if (bClientWasMoving)
		{
			NetworkPrediction->SendStopMove();
			bClientWasMoving = false;
		}
		return;
	}

	bClientWasMoving = true;
	NetworkPrediction->SendMoveInput(ClampedMoveInput);
}

void ANPReplicatedStablePhysicsPawn::ApplyJumpRequest()
{
	if (HasAuthority())
	{
		Super::ApplyJumpRequest();
	}
	else if (IsLocallyControlled())
	{
		Super::ApplyJumpRequest();
		NetworkPrediction->SendJumpRequest();
	}
}

void ANPReplicatedStablePhysicsPawn::ApplyRightHandState(bool bActive)
{
	if (HasAuthority())
	{
		SetServerRightHandState(bActive);
		return;
	}

	if (IsLocallyControlled())
	{
		bLocalRightHandActive = bActive;
		// 손과 Constraint는 즉시 예측하고, 실제 소유와 제출 판정은 서버 상태로 확정합니다.
		SetRightHandVisualState(bActive);
		RightHandGrab->SetGameplayNotificationsEnabled(!bActive);
		RightHandGrab->SetGrabSimulationEnabled(true);
		RightHandGrab->SetGrabRequested(bActive);
		bAwaitingServerGrabConfirmation = bActive;
		LocalGrabPredictionTimeRemaining = bActive
			? LocalGrabPredictionTimeout
			: 0.0f;
		if (!bActive)
		{
			bAwaitingServerGrabConfirmation = false;
			RightHandGrab->SetGameplayNotificationsEnabled(true);
		}
		ServerSetRightHandActive(bActive);
	}
}

void ANPReplicatedStablePhysicsPawn::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
}

FRotator ANPReplicatedStablePhysicsPawn::GetTargetViewRotation() const
{
	if (IsLocallyControlled() && Controller)
	{
		return Controller->GetControlRotation();
	}

	return ReplicatedViewRotation;
}

void ANPReplicatedStablePhysicsPawn::ServerSetViewRotation_Implementation(
	uint16 CompressedYaw,
	uint16 CompressedPitch)
{
	SetReplicatedViewRotation(FRotator(
		FRotator::DecompressAxisFromShort(CompressedPitch),
		FRotator::DecompressAxisFromShort(CompressedYaw),
		0.0f));
}

void ANPReplicatedStablePhysicsPawn::ServerSetRightHandActive_Implementation(
	bool bActive)
{
	SetServerRightHandState(bActive);
}

void ANPReplicatedStablePhysicsPawn::OnRep_RightHandActive()
{
	SetRightHandVisualState(
		IsLocallyControlled()
			? bLocalRightHandActive
			: bReplicatedRightHandActive);
}

void ANPReplicatedStablePhysicsPawn::OnRep_GrabState()
{
	if (IsLocallyControlled() && !bLocalRightHandActive)
	{
		RightHandGrab->ClearReplicatedGrab();
		ClearRightHandIKWorldTarget();
		return;
	}

	if (!IsReplicatedGrabActive())
	{
		RightHandGrab->ClearReplicatedGrab();
		ClearRightHandIKWorldTarget();
		return;
	}

	bAwaitingServerGrabConfirmation = false;
	LocalGrabPredictionTimeRemaining = 0.0f;

	UPrimitiveComponent* GrabbedComponent = ResolveReplicatedGrabbedComponent();
	if (!GrabbedComponent)
	{
		RightHandGrab->ClearReplicatedGrab();
		ClearRightHandIKWorldTarget();
		return;
	}

	if (IsLocallyControlled())
	{
		RightHandGrab->SetGameplayNotificationsEnabled(true);
		RightHandGrab->SetGrabSimulationEnabled(true);
		RightHandGrab->ApplyReplicatedGrab(
			GrabbedComponent,
			ReplicatedGrabState.GrabbedBoneName,
			ReplicatedGrabState.ConstraintFrame1,
			ReplicatedGrabState.ConstraintFrame2);
		return;
	}

	RightHandGrab->ApplyReplicatedGrabState(
		GrabbedComponent,
		ReplicatedGrabState.GrabbedBoneName);
}

void ANPReplicatedStablePhysicsPawn::OnRep_ExternallyGrabbed()
{
	NetworkPrediction->SetExternalGrabActive(bExternallyGrabbed);
}

void ANPReplicatedStablePhysicsPawn::UpdateReplicatedGrabVisualTarget()
{
	if (HasAuthority() || !IsReplicatedGrabActive())
	{
		ClearRightHandIKWorldTarget();
		return;
	}

	UPrimitiveComponent* GrabbedComponent = ResolveReplicatedGrabbedComponent();
	FBodyInstance* GrabbedBody = GrabbedComponent
		? GrabbedComponent->GetBodyInstance(ReplicatedGrabState.GrabbedBoneName)
		: nullptr;
	FBodyInstance* HandBody = PhysicsMesh->GetBodyInstance(RightHandBoneName);
	if (!GrabbedBody || !HandBody)
	{
		ClearRightHandIKWorldTarget();
		return;
	}

	const FTransform GrabWorldFrame = ReplicatedGrabState.ConstraintFrame2
		* GrabbedBody->GetUnrealWorldTransform();
	const FTransform DesiredHandBodyWorld =
		ReplicatedGrabState.ConstraintFrame1.Inverse() * GrabWorldFrame;
	const FTransform HandSocketFrame = PhysicsMesh
		->GetSocketTransform(RightHandBoneName)
		.GetRelativeTransform(HandBody->GetUnrealWorldTransform());
	const FTransform DesiredHandSocketWorld =
		HandSocketFrame * DesiredHandBodyWorld;
	SetRightHandIKWorldTarget(DesiredHandSocketWorld.GetLocation());
}

void ANPReplicatedStablePhysicsPawn::UpdateLocalPredictedGrab(float DeltaSeconds)
{
	if (HasAuthority()
		|| !IsLocallyControlled()
		|| !bAwaitingServerGrabConfirmation
		|| IsReplicatedGrabActive())
	{
		return;
	}

	LocalGrabPredictionTimeRemaining = FMath::Max(
		LocalGrabPredictionTimeRemaining - DeltaSeconds,
		0.0f);
	if (LocalGrabPredictionTimeRemaining > 0.0f)
	{
		return;
	}

	bAwaitingServerGrabConfirmation = false;
	RightHandGrab->SetGrabSimulationEnabled(false);
	RightHandGrab->SetGameplayNotificationsEnabled(true);
}

void ANPReplicatedStablePhysicsPawn::HandleGrabbedComponentChanged(
	UPrimitiveComponent* NewGrabbedComponent)
{
	ANPBaseRelic* NewHeldRelic = IsValid(NewGrabbedComponent)
		? Cast<ANPBaseRelic>(NewGrabbedComponent->GetOwner())
		: nullptr;
	AbilitySystem->SetHeldRelic(NewHeldRelic);

	ANPReplicatedStablePhysicsPawn* NewGrabbedPawn = IsValid(NewGrabbedComponent)
		? Cast<ANPReplicatedStablePhysicsPawn>(NewGrabbedComponent->GetOwner())
		: nullptr;
	if (NewGrabbedPawn == this)
	{
		NewGrabbedPawn = nullptr;
	}

	if (ExternallyGrabbedTargetPawn != NewGrabbedPawn)
	{
		if (IsValid(ExternallyGrabbedTargetPawn))
		{
			ExternallyGrabbedTargetPawn->RemoveExternalGrabber();
		}

		ExternallyGrabbedTargetPawn = NewGrabbedPawn;
		if (IsValid(ExternallyGrabbedTargetPawn))
		{
			ExternallyGrabbedTargetPawn->AddExternalGrabber();
		}
	}

	if (IsValid(NewGrabbedComponent))
	{
		ReplicatedGrabState.GrabbedActor = NewGrabbedComponent->GetOwner();
		ReplicatedGrabState.GrabbedComponentName = NewGrabbedComponent->GetFName();
		ReplicatedGrabState.GrabbedBoneName = RightHandGrab->GetGrabbedBoneName();
		ReplicatedGrabState.ConstraintFrame1 = RightHandGrab->GetGrabConstraintFrame(
			EConstraintFrame::Frame1);
		ReplicatedGrabState.ConstraintFrame2 = RightHandGrab->GetGrabConstraintFrame(
			EConstraintFrame::Frame2);
		ReplicatedServerHandWorldLocation =
			PhysicsMesh->GetSocketLocation(RightHandBoneName);

		if (NewHeldRelic)
		{
			NewHeldRelic->SetLastCarrierPlayerState(
				GetPlayerState<ANPPlayerState>());
		}
	}
	else
	{
		ReplicatedGrabState = FReplicatedStableGrabState();
	}
	ForceNetUpdate();
}

void ANPReplicatedStablePhysicsPawn::AddExternalGrabber()
{
	++ExternalGrabberCount;
	if (bExternallyGrabbed)
	{
		return;
	}

	bExternallyGrabbed = true;
	ForceNetUpdate();
}

void ANPReplicatedStablePhysicsPawn::RemoveExternalGrabber()
{
	ExternalGrabberCount = FMath::Max(ExternalGrabberCount - 1, 0);
	if (ExternalGrabberCount > 0 || !bExternallyGrabbed)
	{
		return;
	}

	bExternallyGrabbed = false;
	ForceNetUpdate();
}

AActor* ANPReplicatedStablePhysicsPawn::GetHeldRelic_Implementation() const
{
	return Cast<ANPBaseRelic>(ReplicatedGrabState.GrabbedActor);
}

UPrimitiveComponent* ANPReplicatedStablePhysicsPawn::ResolveReplicatedGrabbedComponent() const
{
	if (!IsValid(ReplicatedGrabState.GrabbedActor))
	{
		return nullptr;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	ReplicatedGrabState.GrabbedActor->GetComponents<UPrimitiveComponent>(
		PrimitiveComponents);
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (PrimitiveComponent
			&& PrimitiveComponent->GetFName()
				== ReplicatedGrabState.GrabbedComponentName)
		{
			return PrimitiveComponent;
		}
	}

	return nullptr;
}

void ANPReplicatedStablePhysicsPawn::UpdateViewRotationReplication(float DeltaSeconds)
{
	if (HasAuthority())
	{
		if (IsLocallyControlled())
		{
			SetReplicatedViewRotation(Super::GetTargetViewRotation());
		}
		return;
	}

	if (!IsLocallyControlled() || !Controller)
	{
		return;
	}

	ViewRotationSendAccumulator += DeltaSeconds;
	if (ViewRotationSendAccumulator < ViewRotationSendInterval)
	{
		return;
	}

	ViewRotationSendAccumulator -= ViewRotationSendInterval;
	const FRotator ViewRotation = Controller->GetControlRotation();
	ServerSetViewRotation(
		FRotator::CompressAxisToShort(ViewRotation.Yaw),
		FRotator::CompressAxisToShort(ViewRotation.Pitch));
}

void ANPReplicatedStablePhysicsPawn::SetReplicatedViewRotation(
	const FRotator& NewViewRotation)
{
	ReplicatedViewRotation = FRotator(
		FMath::Clamp(
			FRotator::NormalizeAxis(NewViewRotation.Pitch),
			-89.9f,
			89.9f),
		FRotator::NormalizeAxis(NewViewRotation.Yaw),
		0.0f);
}

void ANPReplicatedStablePhysicsPawn::SetServerRightHandState(bool bActive)
{
	bReplicatedRightHandActive = bActive;
	Super::ApplyRightHandState(bActive);
	ForceNetUpdate();
}
