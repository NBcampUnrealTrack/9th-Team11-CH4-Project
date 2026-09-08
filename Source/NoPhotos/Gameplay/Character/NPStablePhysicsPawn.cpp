#include "Gameplay/Character/NPStablePhysicsPawn.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Core/GameplayTag/NPGameplayTags.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsControlComponent.h"
#include "TimerManager.h"
#include "Gameplay/Character/Component/NPStablePhysicsDebugComponent.h"
#include "Gameplay/Character/Component/NPStablePhysicsGrabComponent.h"
#include "Gameplay/Character/Component/NPStablePhysicsMovementComponent.h"
#include "Gameplay/Relic/Components/NPSwingableRelicComponent.h"
#include "Gameplay/Photo/NPPhotoLog.h"
#include "Core/Audio/NPSoundSubsystem.h"

ANPStablePhysicsPawn::ANPStablePhysicsPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	PhysicsMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PhysicsMesh"));
	SetRootComponent(PhysicsMesh);
	PhysicsMesh->SetCollisionProfileName(TEXT("Ragdoll"));
	PhysicsMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	PhysicsMesh->SetEnableGravity(true);
	PhysicsMesh->PhysicsTransformUpdateMode = EPhysicsTransformUpdateMode::SimulationUpatesComponentTransform;
	PhysicsMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	CameraRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CameraRoot"));
	CameraRoot->SetupAttachment(PhysicsMesh);
	CameraRoot->SetUsingAbsoluteRotation(true);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(CameraRoot);
	CameraBoom->PrimaryComponentTick.TickGroup = TG_PostPhysics;
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bInheritRoll = false;
	CameraBoom->bDoCollisionTest = true;
	CameraBoom->ProbeSize = 14.0f;
	CameraBoom->ProbeChannel = ECC_Camera;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 20.0f;
	CameraBoom->CameraLagMaxDistance = 30.0f;
	CameraBoom->bEnableCameraRotationLag = false;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	PhysicalAnimation = CreateDefaultSubobject<UPhysicalAnimationComponent>(TEXT("PhysicalAnimation"));
	PhysicsControl = CreateDefaultSubobject<UPhysicsControlComponent>(TEXT("PhysicsControl"));
	PhysicsMovement = CreateDefaultSubobject<UNPStablePhysicsMovementComponent>(TEXT("NPPhysicsMovement"));
	PhysicsDebug = CreateDefaultSubobject<UNPStablePhysicsDebugComponent>(TEXT("NPPhysicsDebug"));
	PhysicsControl->AddTickPrerequisiteComponent(PhysicsMovement);

	RightHandGrab = CreateDefaultSubobject<UNPStablePhysicsGrabComponent>(TEXT("RightHandGrab"));
	RightHandGrab->SetupAttachment(PhysicsMesh);

	PhysicalBodyGroups = {
		FStablePhysicalBodyGroupSettings(TEXT("pelvis"), 2500.0f, 300.0f, 0.0f),
		FStablePhysicalBodyGroupSettings(TEXT("thigh_l"), 6000.0f, 700.0f, 0.0f),
		FStablePhysicalBodyGroupSettings(TEXT("thigh_r"), 6000.0f, 700.0f, 0.0f),
		FStablePhysicalBodyGroupSettings(TEXT("spine_02"), 700.0f, 100.0f, 0.0f),
		FStablePhysicalBodyGroupSettings(TEXT("neck_01"), 350.0f, 50.0f, 0.0f),
		FStablePhysicalBodyGroupSettings(TEXT("upperarm_l"), 250.0f, 35.0f, 0.0f),
		FStablePhysicalBodyGroupSettings(TEXT("upperarm_r"), 250.0f, 35.0f, 0.0f),
		FStablePhysicalBodyGroupSettings(TEXT("hand_l"), 80.0f, 15.0f, 0.0f),
		FStablePhysicalBodyGroupSettings(TEXT("hand_r"), 80.0f, 15.0f, 0.0f)
	};
}

void ANPStablePhysicsPawn::BeginPlay()
{
	Super::BeginPlay();

	SeamlessTravelTransitionHandle =
		FWorldDelegates::OnSeamlessTravelTransition.AddUObject(
			this,
			&ANPStablePhysicsPawn::HandleSeamlessTravelTransition);

	ApplyCharacterProfile();
	PhysicsMovement->Initialize(PhysicsMesh, CharacterForwardYawOffset);
	RightHandGrab->Initialize(PhysicsMesh, RightHandBoneName);
	PhysicsMovement->OnJumpApplied.AddUObject(
		RightHandGrab,
		&UNPStablePhysicsGrabComponent::NotifyJumpIntent);
	InitializePhysicalAnimation();
	PhysicsMovement->InitializeFacingControl(PhysicsControl);
	DefaultCameraArmLength = CameraBoom->TargetArmLength;
	DefaultCameraFOV = FollowCamera->FieldOfView;
	CurrentCameraTargetHeight = CameraTargetHeight;
	UpdateCameraTarget();
	UpdateFacingTarget();
	CameraBoom->AddTickPrerequisiteActor(this);
	PhysicsDebug->AddTickPrerequisiteActor(this);
	PhysicsDebug->AddTickPrerequisiteComponent(RightHandGrab);
}

void ANPStablePhysicsPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (SeamlessTravelTransitionHandle.IsValid())
	{
		FWorldDelegates::OnSeamlessTravelTransition.Remove(
			SeamlessTravelTransitionHandle);
		SeamlessTravelTransitionHandle.Reset();
	}

	if (PhysicalAnimation)
	{
		PhysicalAnimation->SetComponentTickEnabled(false);
		PhysicalAnimation->Deactivate();
		PhysicalAnimation->SetSkeletalMeshComponent(nullptr);
	}

	GetWorldTimerManager().ClearTimer(TemporaryRagdollTimer);
	GetWorldTimerManager().ClearTimer(TemporaryRagdollRecoveryTimer);
	GetWorldTimerManager().ClearTimer(TemporaryRagdollInputDelayTimer);

	Super::EndPlay(EndPlayReason);
}

void ANPStablePhysicsPawn::HandleSeamlessTravelTransition(
	UWorld* TransitioningWorld)
{
	if (TransitioningWorld != GetWorld() || !PhysicalAnimation)
	{
		return;
	}

	PhysicalAnimation->SetComponentTickEnabled(false);
	PhysicalAnimation->Deactivate();
	PhysicalAnimation->SetSkeletalMeshComponent(nullptr);
}

void ANPStablePhysicsPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	RefreshCharacterProfileIfChanged();
	PhysicsMovement->SetFacingControlEnabled(IsLocallyControlled());
	UpdatePhotoCamera(DeltaSeconds);
	UpdateCameraTarget();
	UpdateFacingTarget();

	UpdateSpinePitch(DeltaSeconds);
	UpdateRightHandIK(DeltaSeconds);
}

FVector ANPStablePhysicsPawn::GetVelocity() const
{
	return PhysicsMovement ? PhysicsMovement->GetVelocity() : FVector::ZeroVector;
}

void ANPStablePhysicsPawn::StopMovementInput()
{
	ApplyMoveInput(FVector::ZeroVector);
}

void ANPStablePhysicsPawn::AddExternalVelocityChange(const FVector& VelocityChange)
{
	if (!HasAuthority() || VelocityChange.IsNearlyZero())
	{
		return;
	}

	ApplyExternalVelocityChangeLocal(VelocityChange);
}

void ANPStablePhysicsPawn::SetExternalVerticalVelocity(float VerticalVelocity)
{
	if (!HasAuthority() || !FMath::IsFinite(VerticalVelocity))
	{
		return;
	}

	SetExternalVerticalVelocityLocal(VerticalVelocity);
}

void ANPStablePhysicsPawn::StartTemporaryRagdoll()
{
	if (HasAuthority())
	{
		BeginTemporaryRagdoll();
	}
}

void ANPStablePhysicsPawn::ApplyExternalVelocityChangeLocal(
	const FVector& VelocityChange)
{
	if (!PhysicsMesh || VelocityChange.IsNearlyZero())
	{
		return;
	}

	StopMovementInput();
	PhysicsMesh->WakeAllRigidBodies();
	PhysicsMesh->AddImpulseToAllBodiesBelow(
		VelocityChange,
		FullBodyRootName,
		true,
		true);
	if (bTemporaryRagdollRecoveryActive)
	{
		BeginTemporaryRagdoll();
	}
}

void ANPStablePhysicsPawn::SetExternalVerticalVelocityLocal(float VerticalVelocity)
{
	if (!PhysicsMesh || !FMath::IsFinite(VerticalVelocity))
	{
		return;
	}

	StopMovementInput();
	PhysicsMesh->WakeAllRigidBodies();
	PhysicsMesh->ForEachBodyBelow(
		FullBodyRootName,
		true,
		false,
		[VerticalVelocity](FBodyInstance* BodyInstance)
		{
			if (!BodyInstance || !BodyInstance->IsInstanceSimulatingPhysics())
			{
				return;
			}

			FVector BodyVelocity = BodyInstance->GetUnrealWorldVelocity();
			BodyVelocity.Z = VerticalVelocity;
			BodyInstance->SetLinearVelocity(BodyVelocity, false);
		});

	if (bTemporaryRagdollRecoveryActive)
	{
		BeginTemporaryRagdoll();
	}
}

void ANPStablePhysicsPawn::BeginTemporaryRagdoll()
{
	if (!PhysicsMesh || !PhysicalAnimation || !PhysicsMovement)
	{
		return;
	}

	bTemporaryRagdollActive = true;
	bTemporaryRagdollRecoveryActive = false;
	TemporaryRagdollSettleStartTime = -1.0;
	TemporaryRagdollAlignmentStartTime = -1.0;
	StopMovementInput();
	PhysicsMovement->SetTemporaryRagdollRecoveryActive(false);
	PhysicsMovement->SetTemporaryRagdollActive(true);

	FPhysicalAnimationData RagdollData;
	RagdollData.bIsLocalSimulation = true;
	PhysicalAnimation->ApplyPhysicalAnimationSettingsBelow(
		FullBodyRootName,
		RagdollData,
		true);
	if (FBodyInstance* PelvisBody =
		PhysicsMesh->GetBodyInstance(FullBodyRootName))
	{
		PelvisBody->bLockXRotation = false;
		PelvisBody->bLockYRotation = false;
		PelvisBody->bLockZRotation = false;
		PelvisBody->SetDOFLock(EDOFMode::SixDOF);
	}

	GetWorldTimerManager().ClearTimer(TemporaryRagdollTimer);
	GetWorldTimerManager().ClearTimer(TemporaryRagdollRecoveryTimer);
	GetWorldTimerManager().ClearTimer(TemporaryRagdollInputDelayTimer);
	GetWorldTimerManager().SetTimer(
		TemporaryRagdollTimer,
		this,
		&ANPStablePhysicsPawn::WaitForTemporaryRagdollSettle,
		FMath::Max(TemporaryRagdollMinimumDuration, 0.01f),
		false);
}

void ANPStablePhysicsPawn::WaitForTemporaryRagdollSettle()
{
	if (!bTemporaryRagdollActive || !PhysicsMesh || !GetWorld())
	{
		return;
	}

	const float PelvisSpeed = PhysicsMesh->GetPhysicsLinearVelocity(
		FullBodyRootName).Size();
	const double CurrentTime = GetWorld()->GetTimeSeconds();
	if (PelvisSpeed > TemporaryRagdollMaximumPelvisSpeed)
	{
		TemporaryRagdollSettleStartTime = -1.0;
	}
	else if (TemporaryRagdollSettleStartTime < 0.0)
	{
		TemporaryRagdollSettleStartTime = CurrentTime;
	}

	if (TemporaryRagdollSettleStartTime >= 0.0
		&& CurrentTime - TemporaryRagdollSettleStartTime
			>= TemporaryRagdollRequiredSettleDuration)
	{
		EndTemporaryRagdoll();
		return;
	}

	GetWorldTimerManager().SetTimer(
		TemporaryRagdollTimer,
		this,
		&ANPStablePhysicsPawn::WaitForTemporaryRagdollSettle,
		0.05f,
		false);
}

void ANPStablePhysicsPawn::EndTemporaryRagdoll()
{
	if (!bTemporaryRagdollActive)
	{
		return;
	}

	bTemporaryRagdollActive = false;
	bTemporaryRagdollRecoveryActive = true;
	TemporaryRagdollAlignmentStartTime = -1.0;
	ApplyPhysicalAnimationGroups();
	PhysicsMovement->SetTemporaryRagdollActive(false);
	PhysicsMovement->SetTemporaryRagdollRecoveryActive(true);
	FinishTemporaryRagdollRecovery();
}

void ANPStablePhysicsPawn::FinishTemporaryRagdollRecovery()
{
	if (bTemporaryRagdollActive || !PhysicsMesh || !GetWorld())
	{
		return;
	}

	const float UprightDot = PhysicsMovement->GetPelvisUprightDot();
	const double CurrentTime = GetWorld()->GetTimeSeconds();
	if (UprightDot < FMath::Cos(FMath::DegreesToRadians(
		FMath::Clamp(
			TemporaryRagdollUprightAngleTolerance,
			0.0f,
			90.0f))))
	{
		TemporaryRagdollAlignmentStartTime = -1.0;
	}
	else if (TemporaryRagdollAlignmentStartTime < 0.0)
	{
		TemporaryRagdollAlignmentStartTime = CurrentTime;
	}

	if (TemporaryRagdollAlignmentStartTime < 0.0
		|| CurrentTime - TemporaryRagdollAlignmentStartTime
			< TemporaryRagdollRequiredAlignmentDuration)
	{
		GetWorldTimerManager().SetTimer(
			TemporaryRagdollRecoveryTimer,
			this,
			&ANPStablePhysicsPawn::FinishTemporaryRagdollRecovery,
			0.05f,
			false);
		return;
	}

	CompleteTemporaryRagdollRecovery();
}

void ANPStablePhysicsPawn::CompleteTemporaryRagdollRecovery()
{
	GetWorldTimerManager().ClearTimer(TemporaryRagdollTimer);
	GetWorldTimerManager().ClearTimer(TemporaryRagdollRecoveryTimer);
	ConfigurePelvisStability();
	const float InputDelay = FMath::Max(
		TemporaryRagdollInputDelay,
		0.0f);
	if (InputDelay <= UE_SMALL_NUMBER)
	{
		FinishTemporaryRagdollInputDelay();
		return;
	}

	GetWorldTimerManager().SetTimer(
		TemporaryRagdollInputDelayTimer,
		this,
		&ANPStablePhysicsPawn::FinishTemporaryRagdollInputDelay,
		InputDelay,
		false);
}

void ANPStablePhysicsPawn::FinishTemporaryRagdollInputDelay()
{
	if (!bTemporaryRagdollActive && PhysicsMovement)
	{
		bTemporaryRagdollRecoveryActive = false;
		PhysicsMovement->SetTemporaryRagdollRecoveryActive(false);
	}
}

bool ANPStablePhysicsPawn::BeginRelicSwing(
	const FNPRelicSwingSettings& Settings)
{
	if (bRelicSwingActive
		|| !PhysicsMovement
		|| !RightHandGrab
		|| !RightHandGrab->IsHoldingObject())
	{
		return false;
	}

	bRelicSwingActive = true;
	RightHandGrab->BeginAbilityGrip(
		Settings.bPreventGrabConstraintBreak,
		Settings.bDisableHeldRelicGravity,
		Settings.HeldRelicMass);
	PhysicsMovement->BeginRelicSwingRotation(
		Settings.Torque,
		Settings.MaxAngularSpeed);
	ApplyRelicSwingPhysicalAnimation(Settings);
	return true;
}

void ANPStablePhysicsPawn::EndRelicSwing()
{
	if (!bRelicSwingActive)
	{
		return;
	}

	bRelicSwingActive = false;
	PhysicsMovement->EndRelicSwingRotation();
	RightHandGrab->EndAbilityGrip();
	ApplyPhysicalAnimationGroups();
}

bool ANPStablePhysicsPawn::PlayPhotoShotMontage()
{
	if (!PhysicsMesh)
	{
		UE_LOG(LogNPPhoto, Error, TEXT("[Montage] PhysicsMesh is null. Pawn=%s"), *GetNameSafe(this));
		return false;
	}
	if (!PhotoShotMontage)
	{
		UE_LOG(LogNPPhoto, Warning, TEXT("[Montage] PhotoShotMontage is not assigned. Pawn=%s"), *GetNameSafe(this));
		return false;
	}

	UAnimInstance* AnimInstance = PhysicsMesh->GetAnimInstance();
	if (!AnimInstance)
	{
		UE_LOG(LogNPPhoto, Error, TEXT("[Montage] AnimInstance is null. Mesh=%s"), *GetNameSafe(PhysicsMesh));
		return false;
	}
	if (AnimInstance->Montage_IsPlaying(PhotoShotMontage))
	{
		UE_LOG(LogNPPhoto, Warning, TEXT("[Montage] PhotoShotMontage is already playing. Montage=%s"), *GetNameSafe(PhotoShotMontage));
		return false;
	}

	const float Duration = AnimInstance->Montage_Play(PhotoShotMontage);
	UE_LOG(
		LogNPPhoto,
		Log,
		TEXT("[Montage] Montage_Play result. Montage=%s Duration=%.2f"),
		*GetNameSafe(PhotoShotMontage),
		Duration);
	return Duration > 0.0f;
}

void ANPStablePhysicsPawn::BroadcastPhotoShutterSound(const FVector& SoundLocation)
{
	if (!HasAuthority())
	{
		return;
	}

	MulticastPlayPhotoShutterSound(SoundLocation);
}

void ANPStablePhysicsPawn::MulticastPlayPhotoShutterSound_Implementation(
	const FVector_NetQuantize10 SoundLocation)
{
	if (!PhotoShutterSound)
	{
		UE_LOG(
			LogNPPhoto,
			Warning,
			TEXT("[Audio] PhotoShutterSound is not assigned. Pawn=%s"),
			*GetNameSafe(this));
		return;
	}

	if (UNPSoundSubsystem* SoundSubsystem = UNPSoundSubsystem::Get(this))
	{
		SoundSubsystem->PlaySFXAtLocation(
			PhotoShutterSound,
			SoundLocation,
			FRotator::ZeroRotator,
			1.0f,
			1.0f,
			0.0f,
			PhotoShutterAttenuation);
	}
}

void ANPStablePhysicsPawn::SetPhotoViewActive(const bool bActive)
{
	if (!IsLocallyControlled() || bPhotoViewActive == bActive)
	{
		return;
	}

	bPhotoViewActive = bActive;
	if (PhysicsMesh)
	{
		// 다른 플레이어에게는 계속 보이고, 이 Pawn을 소유한 로컬 화면에서만 숨깁니다.
		PhysicsMesh->SetOwnerNoSee(bPhotoViewActive || bRelicAimViewActive);
	}
	UE_LOG(
		LogNPPhoto,
		Log,
		TEXT("[PhotoMode] Pawn view changed. Pawn=%s Active=%s"),
		*GetNameSafe(this),
		bPhotoViewActive ? TEXT("true") : TEXT("false"));
}

void ANPStablePhysicsPawn::SetRelicAimViewActive(const bool bActive)
{
	if (!IsLocallyControlled() || bRelicAimViewActive == bActive)
	{
		return;
	}

	bRelicAimViewActive = bActive;
	if (PhysicsMesh)
	{
		PhysicsMesh->SetOwnerNoSee(bPhotoViewActive || bRelicAimViewActive);
	}
}

bool ANPStablePhysicsPawn::IsPhotoViewReady() const
{
	return bPhotoViewActive
		&& FMath::IsNearlyEqual(CameraBoom->TargetArmLength, PhotoCameraArmLength, 5.0f)
		&& FMath::IsNearlyEqual(CurrentCameraTargetHeight, PhotoCameraTargetHeight, 5.0f)
		&& FMath::IsNearlyEqual(FollowCamera->FieldOfView, PhotoCameraFOV, 1.0f);
}

float ANPStablePhysicsPawn::GetAnimationGroundSpeed() const
{
	FVector HorizontalVelocity = GetVelocity();
	HorizontalVelocity.Z = 0.0f;
	const float GroundSpeed = HorizontalVelocity.Size();
	return GroundSpeed >= AnimationSpeedDeadZone ? GroundSpeed : 0.0f;
}

float ANPStablePhysicsPawn::GetAnimationMovementDirection() const
{
	FVector HorizontalVelocity = GetVelocity();
	HorizontalVelocity.Z = 0.0f;
	if (HorizontalVelocity.SizeSquared() < FMath::Square(AnimationSpeedDeadZone))
	{
		return 0.0f;
	}

	const FVector MovementDirection = HorizontalVelocity.GetSafeNormal();
	const FVector VisualForward = GetVisualFacingRotation().Vector();
	const FVector VisualRight = FVector::CrossProduct(FVector::UpVector, VisualForward);
	return FMath::RadiansToDegrees(FMath::Atan2(
		FVector::DotProduct(MovementDirection, VisualRight),
		FVector::DotProduct(MovementDirection, VisualForward)));
}

FRotator ANPStablePhysicsPawn::GetVisualFacingRotation() const
{
	return GetVisualForwardDirection().Rotation();
}

FVector ANPStablePhysicsPawn::GetVisualForwardDirection() const
{
	return PhysicsMovement
		? PhysicsMovement->GetCurrentFacingDirection()
		: FVector::ForwardVector;
}

FVector ANPStablePhysicsPawn::GetViewForwardDirection() const
{
	return FRotator(0.0f, GetTargetViewRotation().Yaw, 0.0f).Vector();
}

FRotator ANPStablePhysicsPawn::GetTargetViewRotation() const
{
	return Controller
		? Controller->GetControlRotation()
		: GetVisualFacingRotation();
}

void ANPStablePhysicsPawn::ApplyCharacterProfile()
{
	if (!CharacterProfile)
	{
		return;
	}

	FullBodyRootName = CharacterProfile->PelvisBoneName;
	RightShoulderBoneName = CharacterProfile->RightShoulderBoneName;
	RightUpperArmBoneName = CharacterProfile->RightUpperArmBoneName;
	RightHandBoneName = CharacterProfile->RightHandBoneName;
	CharacterForwardYawOffset = CharacterProfile->CharacterForwardYawOffset;
	RightHandReachDistance = CharacterProfile->RightHandReachDistance;
	RightElbowOutwardDistance = CharacterProfile->RightElbowOutwardDistance;
	RightHandIKBlendSpeed = CharacterProfile->RightHandIKBlendSpeed;
	MaxSpineBendAngle = CharacterProfile->MaxSpineBendAngle;
	MaxSpineLeanBackAngle = CharacterProfile->MaxSpineLeanBackAngle;
	SpineBendStartViewPitch = CharacterProfile->SpineBendStartViewPitch;
	SpineLeanBackStartViewPitch = CharacterProfile->SpineLeanBackStartViewPitch;
	SpinePitchInterpSpeed = CharacterProfile->SpinePitchInterpSpeed;
	PhysicalBodyGroups = CharacterProfile->GetPhysicalBodyGroups();
	AppliedCharacterProfileRevision = CharacterProfile->GetSettingsRevision();
	AppliedCharacterProfile = CharacterProfile;

	PhysicsMovement->ConfigureBoneNames(
		CharacterProfile->PelvisBoneName,
		CharacterProfile->LeftFootBoneName,
		CharacterProfile->RightFootBoneName);
	PhysicsMovement->SetTargetPelvisHeight(CharacterProfile->PelvisHeight);
	PhysicsMovement->SetMaxMoveSpeed(CharacterProfile->MaxMoveSpeed);
	PhysicsMovement->SetClimbAcceleration(CharacterProfile->ClimbAcceleration);
	PhysicsMovement->SetGravityScale(CharacterProfile->GravityScale);
	PhysicsMovement->SetJumpVelocityChange(CharacterProfile->JumpVelocityChange);
	PhysicsMovement->SetJumpCooldown(CharacterProfile->JumpCooldown);
	PhysicsMovement->SetWalkableSlopeAngle(CharacterProfile->WalkableSlopeAngle);
	PhysicsMovement->SetFacingControlSettings(
		CharacterProfile->FacingAngularStrength,
		CharacterProfile->FacingAngularDampingRatio,
		CharacterProfile->MaxFacingTorque,
		CharacterProfile->MaxFacingTargetSpeed);
	RightHandGrab->SetLinearBreakThreshold(
		CharacterProfile->GrabLinearBreakThreshold);
	RightHandGrab->SetReplicatedGrabFrameBlendDuration(
		CharacterProfile->ReplicatedGrabFrameBlendDuration);
	RightHandGrab->SetGrabRetryCooldown(
		CharacterProfile->GrabRetryCooldown);
}

void ANPStablePhysicsPawn::RefreshCharacterProfileIfChanged()
{
	if (!CharacterProfile
		|| (AppliedCharacterProfile.Get() == CharacterProfile
			&& AppliedCharacterProfileRevision == CharacterProfile->GetSettingsRevision()))
	{
		return;
	}

	// PIE 중 프로필을 조정해도 디버그 표시와 실제 Physical Animation을 함께 갱신합니다.
	ApplyCharacterProfile();
	PhysicsMovement->Initialize(PhysicsMesh, CharacterForwardYawOffset);
	RightHandGrab->Initialize(PhysicsMesh, RightHandBoneName);
	InitializePhysicalAnimation();
	PhysicsMovement->InitializeFacingControl(PhysicsControl);
}

void ANPStablePhysicsPawn::InitializePhysicalAnimation()
{
	const UPhysicsAsset* PhysicsAsset = PhysicsMesh->GetPhysicsAsset();
	if (!PhysicsAsset
		|| PhysicsMesh->GetBoneIndex(FullBodyRootName) == INDEX_NONE
		|| PhysicsAsset->FindBodyIndex(FullBodyRootName) == INDEX_NONE)
	{
		return;
	}

	PhysicalAnimation->SetSkeletalMeshComponent(PhysicsMesh);
	ApplyPhysicalAnimationGroups();
	PhysicsMesh->SetAllBodiesBelowSimulatePhysics(FullBodyRootName, true, true);
	PhysicsMesh->WakeAllRigidBodies();
	ConfigurePelvisStability();
}

void ANPStablePhysicsPawn::ApplyPhysicalAnimationGroups()
{
	const UPhysicsAsset* PhysicsAsset = PhysicsMesh->GetPhysicsAsset();
	for (const FStablePhysicalBodyGroupSettings& Group : PhysicalBodyGroups)
	{
		if (Group.RootBodyName.IsNone()
			|| PhysicsMesh->GetBoneIndex(Group.RootBodyName) == INDEX_NONE
			|| !PhysicsAsset
			|| PhysicsAsset->FindBodyIndex(Group.RootBodyName) == INDEX_NONE)
		{
			continue;
		}

		FPhysicalAnimationData AnimationData;
		AnimationData.bIsLocalSimulation = true;
		AnimationData.OrientationStrength = Group.OrientationStrength;
		AnimationData.AngularVelocityStrength = Group.AngularVelocityStrength;
		AnimationData.MaxAngularForce = Group.MaxAngularForce;

		// Pelvis는 이동 제약으로 제어하고, 하위 Body는 뒤쪽 그룹 설정으로 덮어씁니다.
		const bool bIncludeRootBody = Group.RootBodyName != FullBodyRootName;
		PhysicalAnimation->ApplyPhysicalAnimationSettingsBelow(
			Group.RootBodyName,
			AnimationData,
			bIncludeRootBody);
	}
}

void ANPStablePhysicsPawn::ApplyRelicSwingPhysicalAnimation(
	const FNPRelicSwingSettings& Settings)
{
	if (!PhysicalAnimation || !PhysicsMesh)
	{
		return;
	}

	if (PhysicsMesh->GetBoneIndex(RightUpperArmBoneName) != INDEX_NONE)
	{
		FPhysicalAnimationData ArmData;
		ArmData.bIsLocalSimulation = true;
		ArmData.OrientationStrength = Settings.ArmOrientationStrength;
		ArmData.AngularVelocityStrength =
			Settings.ArmAngularVelocityStrength;
		PhysicalAnimation->ApplyPhysicalAnimationSettingsBelow(
			RightUpperArmBoneName,
			ArmData,
			true);
	}

	if (PhysicsMesh->GetBoneIndex(RightHandBoneName) != INDEX_NONE)
	{
		FPhysicalAnimationData HandData;
		HandData.bIsLocalSimulation = true;
		HandData.OrientationStrength = Settings.HandOrientationStrength;
		HandData.AngularVelocityStrength =
			Settings.HandAngularVelocityStrength;
		PhysicalAnimation->ApplyPhysicalAnimationSettingsBelow(
			RightHandBoneName,
			HandData,
			true);
	}
}

void ANPStablePhysicsPawn::ConfigurePelvisStability()
{
	if (FBodyInstance* PelvisBody = PhysicsMesh->GetBodyInstance(FullBodyRootName))
	{
		PelvisBody->bLockXRotation = bLockPelvisTilt;
		PelvisBody->bLockYRotation = bLockPelvisTilt;
		PelvisBody->bLockZRotation = !PhysicsMovement->IsFacingRotationEnabled();
		PelvisBody->SetDOFLock(EDOFMode::SixDOF);
	}
}

void ANPStablePhysicsPawn::UpdateCameraTarget()
{
	if (PhysicsMesh->GetBoneIndex(FullBodyRootName) == INDEX_NONE)
	{
		return;
	}

	const FVector TargetLocation = PhysicsMesh->GetSocketLocation(FullBodyRootName)
		+ FVector::UpVector * CurrentCameraTargetHeight;
	CameraRoot->SetWorldLocation(TargetLocation);
}

void ANPStablePhysicsPawn::UpdatePhotoCamera(const float DeltaSeconds)
{
	const float TargetArmLength = bPhotoViewActive
		? PhotoCameraArmLength
		: bRelicAimViewActive
			? RelicAimCameraArmLength
			: DefaultCameraArmLength;
	const float TargetHeight = bPhotoViewActive
		? PhotoCameraTargetHeight
		: bRelicAimViewActive
			? RelicAimCameraTargetHeight
			: CameraTargetHeight;
	const float TargetFOV = bPhotoViewActive
		? PhotoCameraFOV
		: bRelicAimViewActive
			? RelicAimCameraFOV
			: DefaultCameraFOV;

	CameraBoom->TargetArmLength = FMath::FInterpTo(
		CameraBoom->TargetArmLength,
		TargetArmLength,
		DeltaSeconds,
		PhotoCameraBlendSpeed);
	CurrentCameraTargetHeight = FMath::FInterpTo(
		CurrentCameraTargetHeight,
		TargetHeight,
		DeltaSeconds,
		PhotoCameraBlendSpeed);
	FollowCamera->SetFieldOfView(FMath::FInterpTo(
		FollowCamera->FieldOfView,
		TargetFOV,
		DeltaSeconds,
		PhotoCameraBlendSpeed));
}

void ANPStablePhysicsPawn::UpdateFacingTarget()
{
	const FRotator CameraYawRotation(0.0f, GetTargetViewRotation().Yaw, 0.0f);
	PhysicsMovement->SetFacingDirection(CameraYawRotation.Vector());
}

void ANPStablePhysicsPawn::UpdateRightHandIK(float DeltaSeconds)
{
	const float TargetAlpha = bRightHandActive && !bRelicSwingActive
		? 1.0f
		: 0.0f;
	RightHandIKAlpha = FMath::FInterpTo(
		RightHandIKAlpha,
		TargetAlpha,
		DeltaSeconds,
		RightHandIKBlendSpeed);

	if (PhysicsMesh->GetBoneIndex(RightShoulderBoneName) == INDEX_NONE)
	{
		return;
	}

	const FVector ShoulderLocation = PhysicsMesh->GetSocketLocation(RightShoulderBoneName);
	FRotator HandTargetRotation = GetTargetViewRotation();
	HandTargetRotation.Roll = 0.0f;
	FVector HandForward = HandTargetRotation.Vector();
	FVector HandRight = FRotationMatrix(HandTargetRotation).GetUnitAxis(EAxis::Y);
	FVector HandTargetLocation = ShoulderLocation
		+ HandForward * RightHandReachDistance;
	FVector ElbowTargetLocation = ShoulderLocation
		+ HandForward * (RightHandReachDistance * 0.5f)
		+ HandRight * RightElbowOutwardDistance;

	if (bHasRightHandIKWorldTarget)
	{
		HandTargetLocation = RightHandIKWorldTarget;
		HandForward = (HandTargetLocation - ShoulderLocation).GetSafeNormal();
		HandRight = FVector::CrossProduct(FVector::UpVector, HandForward)
			.GetSafeNormal();
		if (HandRight.IsNearlyZero())
		{
			HandRight = FRotationMatrix(HandTargetRotation).GetUnitAxis(EAxis::Y);
		}
		ElbowTargetLocation = FMath::Lerp(
			ShoulderLocation,
			HandTargetLocation,
			0.5f) + HandRight * RightElbowOutwardDistance;
	}

	const FTransform& MeshTransform = PhysicsMesh->GetComponentTransform();
	RightHandIKLocation = MeshTransform.InverseTransformPosition(HandTargetLocation);
	RightElbowIKLocation = MeshTransform.InverseTransformPosition(ElbowTargetLocation);
}

void ANPStablePhysicsPawn::UpdateSpinePitch(float DeltaSeconds)
{
	CurrentViewPitch = FRotator::NormalizeAxis(GetTargetViewRotation().Pitch);

	float TargetPitch = 0.0f;
	if (CurrentViewPitch <= SpineBendStartViewPitch)
	{
		const float BendRange = FMath::Max(
			SpineBendStartViewPitch + 90.0f,
			KINDA_SMALL_NUMBER);
		const float BendRatio = FMath::Clamp(
			(SpineBendStartViewPitch - CurrentViewPitch) / BendRange,
			0.0f,
			1.0f);
		TargetPitch = -BendRatio * MaxSpineBendAngle;
	}
	else if (CurrentViewPitch >= SpineLeanBackStartViewPitch)
	{
		const float LeanBackRange = FMath::Max(
			90.0f - SpineLeanBackStartViewPitch,
			KINDA_SMALL_NUMBER);
		const float LeanBackRatio = FMath::Clamp(
			(CurrentViewPitch - SpineLeanBackStartViewPitch) / LeanBackRange,
			0.0f,
			1.0f);
		TargetPitch = LeanBackRatio * MaxSpineLeanBackAngle;
	}

	SpinePitch = FMath::FInterpTo(
		SpinePitch,
		TargetPitch,
		DeltaSeconds,
		SpinePitchInterpSpeed);
}

void ANPStablePhysicsPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInputComponent)
	{
		return;
	}

	if (MoveAction)
	{
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ANPStablePhysicsPawn::Move);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &ANPStablePhysicsPawn::Move);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Canceled, this, &ANPStablePhysicsPawn::Move);
	}
	if (LookAction)
	{
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ANPStablePhysicsPawn::Look);
	}
	if (MouseLookAction)
	{
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ANPStablePhysicsPawn::Look);
	}
	if (JumpAction)
	{
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ANPStablePhysicsPawn::Jump);
	}
	if (RightHandAction)
	{
		EnhancedInputComponent->BindAction(RightHandAction, ETriggerEvent::Started, this, &ANPStablePhysicsPawn::StartRightHand);
		EnhancedInputComponent->BindAction(RightHandAction, ETriggerEvent::Completed, this, &ANPStablePhysicsPawn::StopRightHand);
		EnhancedInputComponent->BindAction(RightHandAction, ETriggerEvent::Canceled, this, &ANPStablePhysicsPawn::StopRightHand);
	}
}

void ANPStablePhysicsPawn::Move(const FInputActionValue& Value)
{
	const FVector2D MovementInput = Value.Get<FVector2D>();
	if (!Controller)
	{
		ApplyMoveInput(FVector::ZeroVector);
		return;
	}
	const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	FVector WorldMoveInput =
		ForwardDirection * MovementInput.Y
		+ RightDirection * MovementInput.X;
	if (bInsideLadderVolume && MovementInput.Y > UE_SMALL_NUMBER)
	{
		WorldMoveInput += FVector::UpVector * MovementInput.Y;
	}
	ApplyMoveInput(WorldMoveInput);
}

void ANPStablePhysicsPawn::Look(const FInputActionValue& Value)
{
	if (const UAbilitySystemComponent* AbilitySystem =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(this);
		AbilitySystem && AbilitySystem->HasMatchingGameplayTag(
			NPGameplayTags::State_CrowdControl_Stunned))
	{
		return;
	}

	const FVector2D LookInput = Value.Get<FVector2D>();
	AddControllerYawInput(LookInput.X);
	AddControllerPitchInput(LookInput.Y);
}

void ANPStablePhysicsPawn::Jump()
{
	ApplyJumpRequest();
}

void ANPStablePhysicsPawn::StartRightHand()
{
	ApplyRightHandState(true);
}

void ANPStablePhysicsPawn::StopRightHand()
{
	ApplyRightHandState(false);
}

void ANPStablePhysicsPawn::ApplyMoveInput(const FVector& WorldMoveInput)
{
	PhysicsMovement->SetMoveInput(WorldMoveInput);
	RightHandGrab->SetMovementIntent(WorldMoveInput);
}

void ANPStablePhysicsPawn::ApplyJumpRequest()
{
	PhysicsMovement->RequestJump();
}

void ANPStablePhysicsPawn::ApplyRightHandState(bool bActive)
{
	SetRightHandVisualState(bActive);
	RightHandGrab->SetGrabRequested(bActive);
}

void ANPStablePhysicsPawn::SetRightHandVisualState(bool bActive)
{
	bRightHandActive = bActive;
}

void ANPStablePhysicsPawn::SetRightHandIKWorldTarget(const FVector& WorldTarget)
{
	bHasRightHandIKWorldTarget = true;
	RightHandIKWorldTarget = WorldTarget;
}

void ANPStablePhysicsPawn::ClearRightHandIKWorldTarget()
{
	bHasRightHandIKWorldTarget = false;
}
