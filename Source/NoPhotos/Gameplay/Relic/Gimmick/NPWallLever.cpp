#include "Gameplay/Relic/Gimmick/NPWallLever.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Gameplay/Interaction/Components/GrabbableComponent.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "UObject/ConstructorHelpers.h"

ANPWallLever::ANPWallLever()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	bReplicates = true;
	SetReplicateMovement(false);
	NetUpdateFrequency = 30.0f;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
	BaseMesh->SetupAttachment(SceneRoot);
	BaseMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	HandleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HandleMesh"));
	HandleMesh->SetupAttachment(SceneRoot);
	HandleMesh->SetMobility(EComponentMobility::Movable);
	HandleMesh->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	HandleMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	HandleMesh->SetEnableGravity(false);
	HandleMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, -InitialAngle));

	HandleConstraint = CreateDefaultSubobject<UPhysicsConstraintComponent>(
		TEXT("HandleConstraint"));
	HandleConstraint->SetupAttachment(SceneRoot);
	HandleConstraint->SetDisableCollision(true);

	GrabbableComponent = CreateDefaultSubobject<UGrabbableComponent>(
		TEXT("GrabbableComponent"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BaseMeshAsset(
		TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/traps/SM_PROP_lever_dungeon_02.SM_PROP_lever_dungeon_02"));
	if (BaseMeshAsset.Succeeded())
	{
		BaseMesh->SetStaticMesh(BaseMeshAsset.Object);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> HandleMeshAsset(
		TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/traps/SM_PROP_lever_handle_dungeon_02.SM_PROP_lever_handle_dungeon_02"));
	if (HandleMeshAsset.Succeeded())
	{
		HandleMesh->SetStaticMesh(HandleMeshAsset.Object);
	}
}

void ANPWallLever::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ANPWallLever, ReplicatedLeverAngle);
	DOREPLIFETIME(ANPWallLever, bIsActivated);
}

void ANPWallLever::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	HandleMesh->SetSimulatePhysics(false);
	HandleMesh->AttachToComponent(
		SceneRoot,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	HandleMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, -InitialAngle));
}

void ANPWallLever::BeginPlay()
{
	Super::BeginPlay();

	HandleMesh->SetSimulatePhysics(false);
	HandleMesh->AttachToComponent(
		SceneRoot,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	HandleRelativeLocation = HandleMesh->GetRelativeLocation();
	HandleZeroRelativeRotation = FQuat::Identity;
	HandleMesh->SetIsReplicated(false);
	HandleMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	GrabbableComponent->OnGrabStarted.AddUObject(
		this, &ANPWallLever::HandleGrabStarted);

	if (HasAuthority())
	{
		ReplicatedLeverAngle = InitialAngle;
		ApplyHandleAngle(InitialAngle);
		InitialHandleWorldRotation = HandleMesh->GetComponentQuat();
		HandleMesh->SetEnableGravity(false);
		HandleMesh->SetAngularDamping(HandleAngularDamping);
		HandleMesh->SetSimulatePhysics(true);
		HandleMesh->SetPhysicsMaxAngularVelocityInDegrees(
			MaxHandleAngularSpeed,
			false);
		ConfigureConstraint();
		HandleMesh->PutRigidBodyToSleep();
	}
	else
	{
		HandleMesh->SetSimulatePhysics(false);
		ApplyHandleAngle(ReplicatedLeverAngle);
		if (bIsActivated)
		{
			GrabbableComponent->SetGrabEnabled(false);
			BroadcastActivationOnce();
			SetActorTickEnabled(false);
		}
	}
}

void ANPWallLever::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || bIsActivated)
	{
		return;
	}

	const float CurrentAngle = FMath::Clamp(
		GetLeverAngle(),
		MinimumAngle,
		InitialAngle);
	if (!FMath::IsNearlyEqual(ReplicatedLeverAngle, CurrentAngle, 0.25f))
	{
		ReplicatedLeverAngle = CurrentAngle;
	}

	if (bHasBeenGrabbed && CurrentAngle <= ActivationAngle)
	{
		ActivateLever(CurrentAngle);
	}
}

void ANPWallLever::ConfigureConstraint()
{
	const float AngularLimit = (InitialAngle - MinimumAngle) * 0.5f;
	const float CenterAngle = (InitialAngle + MinimumAngle) * 0.5f;
	const FQuat CenterRotation =
		FRotator(0.0f, 0.0f, -CenterAngle).Quaternion();
	const FQuat ConstraintRotation =
		SceneRoot->GetComponentQuat()
		* HandleZeroRelativeRotation
		* CenterRotation;

	HandleConstraint->SetWorldLocationAndRotation(
		HandleMesh->GetComponentLocation(),
		ConstraintRotation);
	HandleConstraint->SetLinearXLimit(LCM_Locked, 0.0f);
	HandleConstraint->SetLinearYLimit(LCM_Locked, 0.0f);
	HandleConstraint->SetLinearZLimit(LCM_Locked, 0.0f);
	HandleConstraint->SetAngularSwing1Limit(ACM_Locked, 0.0f);
	HandleConstraint->SetAngularSwing2Limit(ACM_Locked, 0.0f);
	HandleConstraint->SetAngularTwistLimit(ACM_Limited, AngularLimit);
	HandleConstraint->ConstraintInstance.AngularRotationOffset =
		FRotator(0.0f, 0.0f, -AngularLimit);
	HandleConstraint->ConstraintInstance.SetAngularDriveMode(
		EAngularDriveMode::TwistAndSwing);
	HandleConstraint->SetAngularVelocityDriveTwistAndSwing(true, false);
	HandleConstraint->SetAngularVelocityTarget(FVector::ZeroVector);
	HandleConstraint->SetAngularDriveParams(
		0.0f,
		LeverAngularResistance,
		0.0f);
	HandleConstraint->SetConstrainedComponents(
		BaseMesh,
		NAME_None,
		HandleMesh,
		NAME_None);
}

void ANPWallLever::HandleGrabStarted(UPrimitiveComponent*)
{
	if (HasAuthority() && !bIsActivated)
	{
		bHasBeenGrabbed = true;
		HandleMesh->WakeAllRigidBodies();
	}
}

float ANPWallLever::GetLeverAngle() const
{
	const float TravelAngle = FMath::RadiansToDegrees(
		InitialHandleWorldRotation.AngularDistance(
			HandleMesh->GetComponentQuat()));
	return InitialAngle - FMath::Clamp(
		TravelAngle,
		0.0f,
		InitialAngle - MinimumAngle);
}

void ANPWallLever::ActivateLever(float CurrentAngle)
{
	bIsActivated = true;
	ReplicatedLeverAngle = FMath::Clamp(
		CurrentAngle,
		MinimumAngle,
		ActivationAngle);

	GrabbableComponent->SetGrabEnabled(false);
	HandleMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	HandleMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
	HandleConstraint->BreakConstraint();
	HandleMesh->SetSimulatePhysics(false);
	HandleMesh->AttachToComponent(
		SceneRoot,
		FAttachmentTransformRules::KeepWorldTransform);
	ApplyHandleAngle(ReplicatedLeverAngle);

	BroadcastActivationOnce();
	ForceNetUpdate();
	SetActorTickEnabled(false);
}

void ANPWallLever::ApplyHandleAngle(float Angle)
{
	const FQuat AngleRotation =
		FRotator(0.0f, 0.0f, -Angle).Quaternion();
	HandleMesh->SetRelativeLocationAndRotation(
		HandleRelativeLocation,
		HandleZeroRelativeRotation * AngleRotation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
}

void ANPWallLever::BroadcastActivationOnce()
{
	if (bActivationEventBroadcast)
	{
		return;
	}

	bActivationEventBroadcast = true;
	OnLeverActivated.Broadcast();
}

void ANPWallLever::OnRep_LeverState()
{
	if (!HasActorBegunPlay())
	{
		return;
	}

	HandleMesh->SetSimulatePhysics(false);
	ApplyHandleAngle(ReplicatedLeverAngle);

	if (!bIsActivated)
	{
		return;
	}

	GrabbableComponent->SetGrabEnabled(false);
	BroadcastActivationOnce();
	SetActorTickEnabled(false);
}
