#include "Gameplay/Map/CCTV/NPCCTVSensor.h"

#include "AbilitySystemComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/Character/NPStablePhysicsPawn.h"
#include "Gameplay/Character/Component/NPStablePhysicsNetworkPredictionComponent.h"
#include "Gameplay/MapEvents/CCTV/NPCCTVMapEvent.h"
#include "Gameplay/Relic/Components/NPAimableRelicComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

ANPCCTVSensor::ANPCCTVSensor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	MountMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MountMesh"));
	MountMesh->SetupAttachment(SceneRoot);
	MountMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SensorYawPivot = CreateDefaultSubobject<USceneComponent>(TEXT("SensorYawPivot"));
	SensorYawPivot->SetupAttachment(SceneRoot);

	CameraMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CameraMesh"));
	CameraMesh->SetupAttachment(SensorYawPivot);
	CameraMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	StatusIndicatorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StatusIndicatorMesh"));
	StatusIndicatorMesh->SetupAttachment(SensorYawPivot);
	StatusIndicatorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SensorLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("SensorLight"));
	SensorLight->SetupAttachment(SensorYawPivot);
	SensorLight->SetMobility(EComponentMobility::Movable);
	SensorLight->SetAttenuationRadius(2500.0f);
	SensorLight->SetInnerConeAngle(18.0f);
	SensorLight->SetOuterConeAngle(22.0f);
	SensorLight->SetIntensity(100000.0f);
	SensorLight->SetVolumetricScatteringIntensity(0.0f);

	BeamMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BeamMesh"));
	BeamMesh->SetupAttachment(SensorLight);
	BeamMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BeamMesh->SetGenerateOverlapEvents(false);
	BeamMesh->SetCanEverAffectNavigation(false);
	BeamMesh->SetCastShadow(false);

	BeamFillMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BeamFillMesh"));
	BeamFillMesh->SetupAttachment(SensorLight);
	BeamFillMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BeamFillMesh->SetGenerateOverlapEvents(false);
	BeamFillMesh->SetCanEverAffectNavigation(false);
	BeamFillMesh->SetCastShadow(false);

	GunAimPivot = CreateDefaultSubobject<USceneComponent>(TEXT("GunAimPivot"));
	GunAimPivot->SetupAttachment(SensorYawPivot);

	GunMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GunMesh"));
	GunMesh->SetupAttachment(GunAimPivot);
	GunMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GunMesh->SetGenerateOverlapEvents(false);
	GunMesh->SetHiddenInGame(true);

	GunFireComponent = CreateDefaultSubobject<UNPAimableRelicComponent>(TEXT("GunFireComponent"));

	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
}

void ANPCCTVSensor::BeginPlay()
{
	Super::BeginPlay();

	AbilitySystem->InitAbilityActorInfo(this, this);
	InitialSensorRotation = SensorYawPivot->GetRelativeRotation();
	InitialGunRotation = GunAimPivot->GetRelativeRotation();
	InitialLightIntensity = SensorLight->Intensity;
	SensorLight->SetVolumetricScatteringIntensity(0.0f);
	if (GetNetMode() != NM_DedicatedServer)
	{
		BeamMaterial = BeamMesh->CreateAndSetMaterialInstanceDynamic(0);
		BeamFillMaterial = BeamFillMesh->CreateAndSetMaterialInstanceDynamic(0);
		StatusMaterial = StatusIndicatorMesh->CreateAndSetMaterialInstanceDynamic(0);
	}
	GunFireComponent->SetMuzzleSourceComponent(GunMesh);

	const float Now = GetServerTime();
	if (HasAuthority())
	{
		ApplyCCTVActiveState();
	}
	else
	{
		BP_OnStateChanged(CurrentState);
		ApplyCCTVActiveState();
	}
	UpdatePresentation(Now);
}

void ANPCCTVSensor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bCCTVActive)
	{
		return;
	}

	const float Now = GetServerTime();
	if (HasAuthority())
	{
		UpdateAuthority(Now);
	}
	UpdateRotation(DeltaSeconds, Now);
	UpdatePresentation(Now);
}

void ANPCCTVSensor::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANPCCTVSensor, CurrentState);
	DOREPLIFETIME(ANPCCTVSensor, bCCTVActive);
	DOREPLIFETIME(ANPCCTVSensor, StateStartServerTime);
	DOREPLIFETIME(ANPCCTVSensor, StateEndServerTime);
	DOREPLIFETIME(ANPCCTVSensor, SweepStartServerTime);
	DOREPLIFETIME(ANPCCTVSensor, LockedTarget);
	DOREPLIFETIME(ANPCCTVSensor, TargetLockStartServerTime);
}

UAbilitySystemComponent* ANPCCTVSensor::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void ANPCCTVSensor::SetCCTVActive(const bool bNewActive)
{
	if (!HasAuthority() || bCCTVActive == bNewActive)
	{
		return;
	}

	bCCTVActive = bNewActive;
	const float Now = GetServerTime();
	SweepStartServerTime = Now;
	EnterState(ENPCCTVSensorState::Safe, Now);
	ApplyCCTVActiveState();
	ForceNetUpdate();
}

void ANPCCTVSensor::SetOwningMapEvent(ANPCCTVMapEvent* InOwningMapEvent)
{
	OwningMapEvent = InOwningMapEvent;
}

void ANPCCTVSensor::ApplyCCTVActiveState()
{
	SetActorTickEnabled(bCCTVActive);
	GunMesh->SetHiddenInGame(!bCCTVActive);

	if (!bCCTVActive)
	{
		ClearTarget();
		MovingDurations.Reset();
		SensorYawPivot->SetRelativeRotation(InitialSensorRotation);
		GunAimPivot->SetRelativeRotation(InitialGunRotation);
	}

	UpdatePresentation(GetServerTime());
}

void ANPCCTVSensor::EnterState(
	const ENPCCTVSensorState NewState,
	const float ServerTime)
{
	if (!HasAuthority())
	{
		return;
	}

	CurrentState = NewState;
	StateStartServerTime = ServerTime;
	switch (CurrentState)
	{
	case ENPCCTVSensorState::Safe:
		StateEndServerTime = ServerTime + FMath::Max(0.1f, SafeDuration);
		break;
	case ENPCCTVSensorState::Warning:
		StateEndServerTime = ServerTime + FMath::Max(0.1f, WarningDuration);
		break;
	case ENPCCTVSensorState::Armed:
		StateEndServerTime = ServerTime + FMath::Max(0.1f, ArmedDuration);
		break;
	}

	ClearTarget();
	MovingDurations.Reset();
	LastDetectionServerTime = ServerTime;
	NextTargetScanServerTime = CurrentState == ENPCCTVSensorState::Armed
		? ServerTime + FMath::Max(0.0f, ArmedEntryGraceDuration)
		: ServerTime;
	OnRep_CurrentState();
	ForceNetUpdate();
}

void ANPCCTVSensor::UpdateAuthority(const float ServerTime)
{
	if (ServerTime >= StateEndServerTime)
	{
		switch (CurrentState)
		{
		case ENPCCTVSensorState::Safe:
			EnterState(ENPCCTVSensorState::Warning, ServerTime);
			break;
		case ENPCCTVSensorState::Warning:
			EnterState(ENPCCTVSensorState::Armed, ServerTime);
			break;
		case ENPCCTVSensorState::Armed:
			EnterState(ENPCCTVSensorState::Safe, ServerTime);
			break;
		}
	}

	if (CurrentState != ENPCCTVSensorState::Armed)
	{
		return;
	}

	if (IsValid(LockedTarget))
	{
		if (ServerTime - TargetLockStartServerTime
			>= FMath::Max(0.0f, FireWindupDuration))
		{
			FireAtLockedTarget(ServerTime);
		}
		return;
	}

	if (ServerTime >= NextTargetScanServerTime
		&& ServerTime - LastDetectionServerTime
		>= FMath::Max(0.02f, DetectionInterval))
	{
		UpdateDetection(ServerTime);
	}
}

void ANPCCTVSensor::UpdateDetection(const float ServerTime)
{
	const float SampleDuration = FMath::Clamp(
		ServerTime - LastDetectionServerTime,
		0.0f,
		FMath::Max(0.02f, DetectionInterval) * 2.0f);
	LastDetectionServerTime = ServerTime;

	ANPStablePhysicsPawn* BestTarget = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	bool bBestTargetRagdoll = true;
	TSet<TWeakObjectPtr<ANPStablePhysicsPawn>> EvaluatedPawns;
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		const APlayerController* Controller = Iterator->Get();
		ANPStablePhysicsPawn* Pawn = Controller
			? Cast<ANPStablePhysicsPawn>(Controller->GetPawn())
			: nullptr;
		if (!IsValid(Pawn))
		{
			continue;
		}

		const TWeakObjectPtr<ANPStablePhysicsPawn> PawnKey(Pawn);
		EvaluatedPawns.Add(PawnKey);
		if (!IsPawnVisible(Pawn) || !IsPawnMoving(Pawn))
		{
			MovingDurations.Remove(PawnKey);
			continue;
		}

		float& MovingDuration = MovingDurations.FindOrAdd(PawnKey);
		MovingDuration += SampleDuration;
		if (MovingDuration < FMath::Max(0.0f, MovementConfirmDuration))
		{
			continue;
		}
		if (OwningMapEvent.IsValid()
			&& OwningMapEvent->IsTargetReserved(Pawn, ServerTime))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(
			SensorLight->GetComponentLocation(),
			Pawn->GetActorLocation());
		const bool bPawnRagdoll = Pawn->IsTemporaryRagdollOrRecovering();
		if (!IsValid(BestTarget)
			|| (bBestTargetRagdoll && !bPawnRagdoll)
			|| (bBestTargetRagdoll == bPawnRagdoll
				&& DistanceSquared < BestDistanceSquared))
		{
			BestDistanceSquared = DistanceSquared;
			BestTarget = Pawn;
			bBestTargetRagdoll = bPawnRagdoll;
		}
	}

	for (auto Iterator = MovingDurations.CreateIterator(); Iterator; ++Iterator)
	{
		if (!Iterator.Key().IsValid() || !EvaluatedPawns.Contains(Iterator.Key()))
		{
			Iterator.RemoveCurrent();
		}
	}

	const float ReservationDuration = FMath::Max(0.0f, FireWindupDuration)
		+ (IsValid(GunFireComponent)
			? FMath::Max(0.0f, GunFireComponent->GetAimSettings().FireInterval)
			: 0.0f);
	if (IsValid(BestTarget)
		&& (!OwningMapEvent.IsValid()
			|| OwningMapEvent->TryReserveTarget(
				BestTarget,
				ServerTime,
				ReservationDuration)))
	{
		LockTarget(BestTarget, ServerTime);
	}
}

void ANPCCTVSensor::LockTarget(
	ANPStablePhysicsPawn* NewTarget,
	const float ServerTime)
{
	if (!HasAuthority() || !IsValid(NewTarget))
	{
		return;
	}

	LockedTarget = NewTarget;
	TargetLockStartServerTime = ServerTime;
	MovingDurations.Reset();
	ForceNetUpdate();
}

void ANPCCTVSensor::ClearTarget()
{
	LockedTarget = nullptr;
	TargetLockStartServerTime = 0.0f;
}

void ANPCCTVSensor::FireAtLockedTarget(const float ServerTime)
{
	ANPStablePhysicsPawn* Target = LockedTarget;
	if (!IsValid(Target) || !IsValid(GunFireComponent))
	{
		ClearTarget();
		NextTargetScanServerTime = ServerTime + FMath::Max(0.02f, DetectionInterval);
		ForceNetUpdate();
		return;
	}

	const FVector TargetLocation = GetCurrentTargetLocation(Target);
	UPrimitiveComponent* TargetComponent = Cast<UPrimitiveComponent>(Target->GetRootComponent());
	const bool bFired = GunFireComponent->TryFireAtTarget(
		this,
		AbilitySystem,
		Target,
		TargetComponent,
		TargetLocation);

	ClearTarget();
	NextTargetScanServerTime = ServerTime + (bFired
		? FMath::Max(0.0f, GunFireComponent->GetAimSettings().FireInterval)
		: FMath::Max(0.02f, DetectionInterval));
	ForceNetUpdate();
}

void ANPCCTVSensor::UpdateRotation(
	const float DeltaSeconds,
	const float ServerTime)
{
	FRotator DesiredSensorRotation = InitialSensorRotation;
	if (IsValid(LockedTarget))
	{
		const USceneComponent* Parent = SensorYawPivot->GetAttachParent();
		const FVector TargetLocation = GetCurrentTargetLocation(LockedTarget);
		const FVector WorldDirection = TargetLocation - SensorYawPivot->GetComponentLocation();
		const FVector LocalDirection = Parent
			? Parent->GetComponentTransform().InverseTransformVectorNoScale(WorldDirection)
			: WorldDirection;
		DesiredSensorRotation.Yaw = LocalDirection.Rotation().Yaw;
	}
	else
	{
		const float Phase = 2.0f * PI
			* FMath::Max(0.0f, ServerTime - SweepStartServerTime)
			/ FMath::Max(0.1f, SweepPeriod);
		DesiredSensorRotation.Yaw += FMath::Sin(Phase) * SweepAngle;
	}

	SensorYawPivot->SetRelativeRotation(FMath::RInterpTo(
		SensorYawPivot->GetRelativeRotation(),
		DesiredSensorRotation,
		DeltaSeconds,
		FMath::Max(0.0f, RotationInterpSpeed)));

	FRotator DesiredGunRotation = InitialGunRotation;
	if (IsValid(LockedTarget))
	{
		const FVector LocalDirection = SensorYawPivot->GetComponentTransform()
			.InverseTransformVectorNoScale(
				GetCurrentTargetLocation(LockedTarget) - GunAimPivot->GetComponentLocation());
		DesiredGunRotation = LocalDirection.Rotation();
	}
	GunAimPivot->SetRelativeRotation(FMath::RInterpTo(
		GunAimPivot->GetRelativeRotation(),
		DesiredGunRotation,
		DeltaSeconds,
		FMath::Max(0.0f, RotationInterpSpeed)));
}

void ANPCCTVSensor::UpdatePresentation(const float ServerTime)
{
	const FLinearColor StateColor = bCCTVActive ? GetStateColor() : SafeColor;
	const float StateAge = FMath::Max(0.0f, ServerTime - StateStartServerTime);
	const float FadeAlpha = StateFadeDuration > 0.0f
		? FMath::SmoothStep(0.0f, 1.0f, StateAge / StateFadeDuration)
		: 1.0f;
	float PulseAlpha = 1.0f;
	if (CurrentState == ENPCCTVSensorState::Warning)
	{
		const float BlinkInterval = FMath::Max(0.05f, WarningBlinkInterval);
		PulseAlpha = FMath::Fmod(StateAge, BlinkInterval * 2.0f) < BlinkInterval
			? 1.0f
			: 0.1f;
	}
	const float VisualAlpha = FadeAlpha * PulseAlpha;

	SensorLight->SetLightColor(StateColor);
	SensorLight->SetIntensity(InitialLightIntensity * VisualAlpha);
	if (BeamMaterial)
	{
		BeamMaterial->SetVectorParameterValue(ColorParameterName, StateColor);
		BeamMaterial->SetScalarParameterValue(OpacityParameterName, VisualAlpha);
	}
	if (BeamFillMaterial)
	{
		BeamFillMaterial->SetVectorParameterValue(ColorParameterName, StateColor);
		BeamFillMaterial->SetScalarParameterValue(OpacityParameterName, VisualAlpha);
	}
	if (StatusMaterial)
	{
		StatusMaterial->SetVectorParameterValue(ColorParameterName, StateColor);
		StatusMaterial->SetScalarParameterValue(OpacityParameterName, VisualAlpha);
	}
}

bool ANPCCTVSensor::IsPawnVisible(const ANPStablePhysicsPawn* Pawn) const
{
	const UPrimitiveComponent* PhysicsBody = IsValid(Pawn)
		? Cast<UPrimitiveComponent>(Pawn->GetRootComponent())
		: nullptr;
	if (!PhysicsBody || !SensorLight->AffectsBounds(PhysicsBody->Bounds))
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CCTVDetection), false, this);
	QueryParams.AddIgnoredActor(Pawn);
	return !GetWorld()->LineTraceTestByChannel(
		SensorLight->GetComponentLocation(),
		PhysicsBody->Bounds.Origin,
		DetectionTraceChannel,
		QueryParams);
}

bool ANPCCTVSensor::IsPawnMoving(const ANPStablePhysicsPawn* Pawn) const
{
	UPrimitiveComponent* PhysicsBody = IsValid(Pawn)
		? Cast<UPrimitiveComponent>(Pawn->GetRootComponent())
		: nullptr;
	if (!PhysicsBody)
	{
		return false;
	}

	return PhysicsBody->GetPhysicsLinearVelocity().Size()
		>= FMath::Max(0.0f, LinearMovementThreshold)
		|| PhysicsBody->GetPhysicsAngularVelocityInDegrees().Size()
		>= FMath::Max(0.0f, AngularMovementThreshold);
}

FVector ANPCCTVSensor::GetCurrentTargetLocation(
	const ANPStablePhysicsPawn* Pawn) const
{
	if (!IsValid(Pawn))
	{
		return FVector::ZeroVector;
	}

	if (const UNPStablePhysicsNetworkPredictionComponent* NetworkPrediction =
		Pawn->FindComponentByClass<UNPStablePhysicsNetworkPredictionComponent>())
	{
		FVector LatestClientLocation;
		if (NetworkPrediction->GetLatestClientRootLocation(LatestClientLocation))
		{
			return LatestClientLocation;
		}
	}

	const UPrimitiveComponent* PhysicsBody = Cast<UPrimitiveComponent>(Pawn->GetRootComponent());
	return PhysicsBody ? PhysicsBody->Bounds.Origin : Pawn->GetActorLocation();
}

float ANPCCTVSensor::GetServerTime() const
{
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return GameState
		? GameState->GetServerWorldTimeSeconds()
		: (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
}

FLinearColor ANPCCTVSensor::GetStateColor() const
{
	switch (CurrentState)
	{
	case ENPCCTVSensorState::Warning:
		return WarningColor;
	case ENPCCTVSensorState::Armed:
		return ArmedColor;
	case ENPCCTVSensorState::Safe:
	default:
		return SafeColor;
	}
}

void ANPCCTVSensor::OnRep_CurrentState()
{
	if (HasActorBegunPlay())
	{
		BP_OnStateChanged(CurrentState);
	}
}

void ANPCCTVSensor::OnRep_CCTVActive()
{
	if (HasActorBegunPlay())
	{
		ApplyCCTVActiveState();
	}
}
