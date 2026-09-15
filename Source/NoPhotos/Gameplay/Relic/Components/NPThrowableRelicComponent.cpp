#include "Gameplay/Relic/Components/NPThrowableRelicComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "CollisionQueryParams.h"
#include "Components/AudioComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Core/GameplayTag/NPGameplayTags.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Gameplay/AbilitySystem/Effects/NPKnockbackGameplayEffect.h"
#include "Gameplay/Character/Component/NPStablePhysicsGrabComponent.h"
#include "Gameplay/Character/NPStablePhysicsPawn.h"
#include "Gameplay/Interaction/Components/GrabbableComponent.h"
#include "Gameplay/Relic/Abilities/NPRelicAimAbility.h"
#include "Gameplay/Relic/Abilities/NPThrowableRelicUseAbility.h"
#include "Gameplay/Relic/NPBaseRelic.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicsEngine/BodyInstance.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogNPThrowableRelic, Log, All);

UNPThrowableRelicComponent::UNPThrowableRelicComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	TArray<TSubclassOf<UGameplayAbility>> AbilityClasses;
	AbilityClasses.Add(UNPThrowableRelicUseAbility::StaticClass());
	AbilityClasses.Add(UNPRelicAimAbility::StaticClass());
	SetUseAbilityClasses(AbilityClasses);
	ThrowSettings.KnockbackEffectClass =
		UNPKnockbackGameplayEffect::StaticClass();
}

void UNPThrowableRelicComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UGrabbableComponent* Grabbable = GetOwner()
		? GetOwner()->FindComponentByClass<UGrabbableComponent>()
		: nullptr)
	{
		Grabbable->OnGrabStarted.AddUObject(
			this,
			&ThisClass::HandleRelicGrabStarted);
	}
}

bool UNPThrowableRelicComponent::CanThrow(
	const ANPStablePhysicsPawn* ThrowerPawn) const
{
	const ANPBaseRelic* Relic = Cast<ANPBaseRelic>(GetOwner());
	const UWorld* World = GetWorld();
	const UGrabbableComponent* Grabbable = Relic
		? Relic->FindComponentByClass<UGrabbableComponent>()
		: nullptr;
	const UNPStablePhysicsGrabComponent* HandGrab = ThrowerPawn
		? ThrowerPawn->GetRightHandGrabComponent()
		: nullptr;
	return IsValid(Relic)
		&& !Relic->IsReturned()
		&& IsValid(World)
		&& IsValid(ThrowerPawn)
		&& IsValid(Grabbable)
		&& Grabbable->GetActiveGrabCount() == 1
		&& IsValid(HandGrab)
		&& HandGrab->GetGrabbedComponent()
		&& HandGrab->GetGrabbedComponent()->GetOwner() == Relic
		&& World->GetTimeSeconds() >= NextThrowAllowedTime;
}

bool UNPThrowableRelicComponent::TryThrow(
	ANPStablePhysicsPawn* ThrowerPawn,
	const FVector& CameraLocation,
	const FVector& CameraForward)
{
	ANPBaseRelic* Relic = Cast<ANPBaseRelic>(GetOwner());
	UWorld* World = GetWorld();
	if (!IsValid(Relic)
		|| !Relic->HasAuthority()
		|| !IsValid(World)
		|| !CanThrow(ThrowerPawn))
	{
		return false;
	}

	UPrimitiveComponent* RelicMesh = ResolveRelicMesh();
	UGrabbableComponent* Grabbable =
		Relic->FindComponentByClass<UGrabbableComponent>();
	if (!IsValid(RelicMesh) || !IsValid(Grabbable))
	{
		return false;
	}

	const FVector AimTarget = ResolveAimTarget(
		ThrowerPawn,
		CameraLocation,
		CameraForward);
	const FVector ThrowVelocity = CalculateAimedThrowVelocity(
		RelicMesh->GetComponentLocation(),
		AimTarget,
		CameraForward,
		ThrowerPawn->GetVelocity());
	const FVector AngularVelocity = CalculateAngularVelocityDegrees(
		RelicMesh->GetComponentTransform(),
		ThrowSettings);
	if (ThrowVelocity.IsNearlyZero())
	{
		return false;
	}

	RestorePawnCollision();
	Grabbable->ForceReleaseAllGrabs();
	if (Grabbable->GetActiveGrabCount() != 0
		|| !Relic->ReleaseWithVelocityImpulse(FVector::ZeroVector)
		|| !RelicMesh->IsSimulatingPhysics())
	{
		UE_LOG(LogNPThrowableRelic, Warning,
			TEXT("투척 실패: 그랩 해제 또는 물리 활성화를 확인하세요. Relic=%s Grabs=%d Simulating=%s"),
			*GetNameSafe(Relic), Grabbable->GetActiveGrabCount(),
			RelicMesh->IsSimulatingPhysics() ? TEXT("true") : TEXT("false"));
		return false;
	}

	Relic->SetInstigator(ThrowerPawn);
	ThrowInstigator = ThrowerPawn;
	ThrowSourceAbilitySystem =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(ThrowerPawn);
	RelicMesh->WakeAllRigidBodies();
	RelicMesh->SetPhysicsLinearVelocity(ThrowVelocity, false);
	RelicMesh->SetPhysicsAngularVelocityInDegrees(AngularVelocity, false);
	BeginFlight(RelicMesh);
	NextThrowAllowedTime = World->GetTimeSeconds()
		+ (FMath::IsFinite(ThrowSettings.Cooldown)
			? FMath::Max(0.0f, ThrowSettings.Cooldown)
			: 0.0f);
	StartUseCooldown(FMath::IsFinite(ThrowSettings.Cooldown)
		? FMath::Max(0.0f, ThrowSettings.Cooldown)
		: 0.0f);

	const float GraceTime = FMath::IsFinite(ThrowSettings.PawnCollisionGraceTime)
		? FMath::Max(0.0f, ThrowSettings.PawnCollisionGraceTime)
		: 0.0f;
	if (GraceTime > UE_SMALL_NUMBER)
	{
		MulticastBeginPawnCollisionGrace(GraceTime);
	}

	Relic->ForceNetUpdate();
	UE_LOG(LogNPThrowableRelic, Display,
		TEXT("유물 투척: Relic=%s Thrower=%s Velocity=%s AngularDeg=%s"),
		*GetNameSafe(Relic), *GetNameSafe(ThrowerPawn),
		*ThrowVelocity.ToCompactString(), *AngularVelocity.ToCompactString());
	return true;
}

void UNPThrowableRelicComponent::BeginFlight(UPrimitiveComponent* RelicMesh)
{
	AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority()
		|| !IsValid(World) || !IsValid(RelicMesh))
	{
		return;
	}

	EndFlight(false);
	FlightHitMesh = RelicMesh;
	if (const FBodyInstance* BodyInstance = RelicMesh->GetBodyInstance())
	{
		bPreviousFlightNotifyRigidBodyCollision =
			BodyInstance->bNotifyRigidBodyCollision;
	}
	else
	{
		bPreviousFlightNotifyRigidBodyCollision = false;
	}
	RelicMesh->SetNotifyRigidBodyCollision(true);
	RelicMesh->OnComponentHit.AddUniqueDynamic(
		this,
		&ThisClass::HandleThrownRelicHit);
	bFlightActive = true;
	bFirstImpactPresented = false;

	const float SafeMaximumDuration = FMath::IsFinite(MaximumFlyingSoundDuration)
		? FMath::Max(0.1f, MaximumFlyingSoundDuration)
		: 10.0f;
	World->GetTimerManager().SetTimer(
		FlightTimeoutTimer,
		this,
		&ThisClass::HandleFlightTimeout,
		SafeMaximumDuration,
		false);
	MulticastBeginFlyingAudio();
}

void UNPThrowableRelicComponent::EndFlight(
	const bool bPlayImpactSound,
	const FVector& ImpactLocation)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority() || !bFlightActive)
	{
		return;
	}

	bFlightActive = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FlightTimeoutTimer);
	}
	if (UPrimitiveComponent* HitMesh = FlightHitMesh.Get())
	{
		HitMesh->OnComponentHit.RemoveDynamic(
			this,
			&ThisClass::HandleThrownRelicHit);
		HitMesh->SetNotifyRigidBodyCollision(
			bPreviousFlightNotifyRigidBodyCollision);
	}
	FlightHitMesh.Reset();
	const bool bPresentImpact = bPlayImpactSound && !bFirstImpactPresented;
	bFirstImpactPresented |= bPlayImpactSound;
	MulticastEndFlyingAudio(bPresentImpact, ImpactLocation);
	ThrowInstigator.Reset();
	ThrowSourceAbilitySystem.Reset();
}

void UNPThrowableRelicComponent::HandleThrownRelicHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent*,
	FVector,
	const FHitResult& Hit)
{
	if (!bFlightActive || HitComponent != FlightHitMesh.Get()
		|| !Hit.bBlockingHit)
	{
		return;
	}

	const FVector ImpactLocation = Hit.ImpactPoint.IsNearlyZero()
		? HitComponent->GetComponentLocation()
		: FVector(Hit.ImpactPoint);
	if (TryApplyKnockback(HitComponent, OtherActor, Hit))
	{
		EndFlight(true, ImpactLocation);
		return;
	}

	// 바닥이나 벽에 먼저 맞아도 캐릭터 공격 판정은 재잡기/시간 초과까지 유지합니다.
	if (!bFirstImpactPresented)
	{
		bFirstImpactPresented = true;
		MulticastEndFlyingAudio(true, ImpactLocation);
	}
}

bool UNPThrowableRelicComponent::TryApplyKnockback(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	const FHitResult& Hit)
{
	AActor* OwnerActor = GetOwner();
	AActor* InstigatorActor = ThrowInstigator.Get();
	UAbilitySystemComponent* SourceASC = ThrowSourceAbilitySystem.Get();
	const FNPRelicThrowSettings& Settings = GetThrowSettings();
	if (!Settings.bCanKnockbackCharacters
		|| !IsValid(OwnerActor)
		|| !OwnerActor->HasAuthority()
		|| !IsValid(HitComponent)
		|| !IsValid(OtherActor)
		|| OtherActor == OwnerActor
		|| OtherActor == InstigatorActor
		|| !IsValid(SourceASC)
		|| !Settings.KnockbackEffectClass)
	{
		return false;
	}

	FVector KnockbackDirection =
		HitComponent->GetPhysicsLinearVelocityAtPoint(Hit.ImpactPoint);
	KnockbackDirection.Z = 0.0f;
	const float HorizontalSpeed = KnockbackDirection.Size();
	if (HorizontalSpeed <= UE_SMALL_NUMBER
		|| HorizontalSpeed < FMath::Max(0.0f, Settings.MinimumKnockbackSpeed))
	{
		return false;
	}
	KnockbackDirection /= HorizontalSpeed;

	UAbilitySystemComponent* TargetASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor);
	if (!IsValid(TargetASC))
	{
		return false;
	}
	if (TargetASC->HasMatchingGameplayTag(
		NPGameplayTags::State_CrowdControl_Immune))
	{
		return false;
	}

	FHitResult KnockbackHit = Hit;
	KnockbackHit.TraceStart = Hit.ImpactPoint;
	KnockbackHit.TraceEnd = Hit.ImpactPoint + KnockbackDirection;
	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(OwnerActor);
	EffectContext.AddHitResult(KnockbackHit, true);

	FGameplayEffectSpecHandle EffectSpec = SourceASC->MakeOutgoingSpec(
		Settings.KnockbackEffectClass,
		1.0f,
		EffectContext);
	if (!EffectSpec.IsValid())
	{
		return false;
	}

	EffectSpec.Data->AddDynamicAssetTag(NPGameplayTags::Effect_Knockback);
	EffectSpec.Data->SetSetByCallerMagnitude(
		NPGameplayTags::Data_Knockback_Magnitude,
		FMath::Max(0.0f, Settings.KnockbackStrength));
	SourceASC->ApplyGameplayEffectSpecToTarget(*EffectSpec.Data.Get(), TargetASC);

	if (Settings.ImpactCueTag.IsValid())
	{
		FGameplayCueParameters CueParameters(EffectContext);
		CueParameters.Location = Hit.ImpactPoint;
		CueParameters.Normal = Hit.ImpactNormal;
		TargetASC->ExecuteGameplayCue(Settings.ImpactCueTag, CueParameters);
	}
	return true;
}

FVector UNPThrowableRelicComponent::ResolveAimTarget(
	const ANPStablePhysicsPawn* ThrowerPawn,
	const FVector& CameraLocation,
	const FVector& CameraForward) const
{
	UWorld* World = GetWorld();
	const FVector AimDirection = CameraForward.GetSafeNormal();
	const float TraceDistance = FMath::IsFinite(ThrowSettings.AimTraceDistance)
		? FMath::Max(1.0f, ThrowSettings.AimTraceDistance)
		: 3000.0f;
	const FVector TraceEnd = CameraLocation + AimDirection * TraceDistance;
	if (!IsValid(World) || CameraLocation.ContainsNaN()
		|| AimDirection.IsNearlyZero())
	{
		return TraceEnd;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ThrowableRelicAim), true);
	QueryParams.AddIgnoredActor(ThrowerPawn);
	QueryParams.AddIgnoredActor(GetOwner());
	FHitResult AimHit;
	return World->LineTraceSingleByChannel(
		AimHit,
		CameraLocation,
		TraceEnd,
		ThrowSettings.AimTraceChannel,
		QueryParams)
		? FVector(AimHit.ImpactPoint)
		: TraceEnd;
}

FVector UNPThrowableRelicComponent::CalculateAimedThrowVelocity(
	const FVector& StartLocation,
	const FVector& TargetLocation,
	const FVector& CameraForward,
	const FVector& ThrowerVelocity) const
{
	const float ForwardSpeed = FMath::IsFinite(ThrowSettings.ForwardSpeed)
		? FMath::Max(0.0f, ThrowSettings.ForwardSpeed)
		: 0.0f;
	const float UpwardSpeed = FMath::IsFinite(ThrowSettings.UpwardSpeed)
		? FMath::Max(0.0f, ThrowSettings.UpwardSpeed)
		: 0.0f;
	const float LaunchSpeed = FVector2D(ForwardSpeed, UpwardSpeed).Size();
	const FVector InheritedVelocity = ThrowSettings.bInheritThrowerVelocity
		&& !ThrowerVelocity.ContainsNaN()
		? ThrowerVelocity
		: FVector::ZeroVector;
	if (LaunchSpeed > UE_SMALL_NUMBER
		&& !StartLocation.ContainsNaN()
		&& !TargetLocation.ContainsNaN())
	{
		FVector SuggestedVelocity = FVector::ZeroVector;
		FVector AdjustedTarget = TargetLocation;
		bool bFoundSolution = false;
		for (int32 Iteration = 0; Iteration < 3; ++Iteration)
		{
			UGameplayStatics::FSuggestProjectileVelocityParameters Params(
				this,
				StartLocation,
				AdjustedTarget,
				LaunchSpeed);
			Params.bFavorHighArc = ThrowSettings.bFavorHighArc;
			Params.TraceOption = ESuggestProjVelocityTraceOption::DoNotTrace;
			Params.ActorsToIgnore.Add(GetOwner());
			bFoundSolution = UGameplayStatics::SuggestProjectileVelocity(
				Params,
				SuggestedVelocity);
			if (!bFoundSolution || InheritedVelocity.IsNearlyZero())
			{
				break;
			}

			const FVector2D HorizontalDelta(
				AdjustedTarget.X - StartLocation.X,
				AdjustedTarget.Y - StartLocation.Y);
			const FVector2D HorizontalVelocity(
				SuggestedVelocity.X,
				SuggestedVelocity.Y);
			const float HorizontalSpeed = HorizontalVelocity.Size();
			if (HorizontalSpeed <= UE_SMALL_NUMBER)
			{
				break;
			}
			const float FlightTime = HorizontalDelta.Size() / HorizontalSpeed;
			AdjustedTarget = TargetLocation - InheritedVelocity * FlightTime;
		}
		if (bFoundSolution)
		{
			return SuggestedVelocity + InheritedVelocity;
		}
	}

	// 현재 속도로 목표점에 도달할 탄도 해가 없으면 기존 직접 투척으로 대체합니다.
	return CalculateThrowVelocity(
		CameraForward,
		ThrowerVelocity,
		ThrowSettings);
}

void UNPThrowableRelicComponent::HandleRelicGrabStarted(
	UPrimitiveComponent*)
{
	EndFlight(false);
}

void UNPThrowableRelicComponent::HandleFlightTimeout()
{
	EndFlight(false);
}

void UNPThrowableRelicComponent::MulticastBeginFlyingAudio_Implementation()
{
	if (GetNetMode() == NM_DedicatedServer || !FlyingLoopSound)
	{
		return;
	}

	StopFlyingAudio(false);
	if (UPrimitiveComponent* RelicMesh = ResolveRelicMesh())
	{
		ActiveFlyingAudio = UGameplayStatics::SpawnSoundAttached(
			FlyingLoopSound,
			RelicMesh,
			NAME_None,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset,
			true,
			1.0f,
			1.0f,
			0.0f,
			ThrowSoundAttenuation,
			nullptr,
			true);
	}
}

void UNPThrowableRelicComponent::MulticastEndFlyingAudio_Implementation(
	const bool bPlayImpactSound,
	const FVector_NetQuantize10 ImpactLocation)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	StopFlyingAudio(true);
	if (bPlayImpactSound && ImpactSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			ImpactSound,
			FVector(ImpactLocation),
			1.0f,
			1.0f,
			0.0f,
			ThrowSoundAttenuation);
	}
}

void UNPThrowableRelicComponent::StopFlyingAudio(const bool bFadeOut)
{
	if (!IsValid(ActiveFlyingAudio))
	{
		ActiveFlyingAudio = nullptr;
		return;
	}

	const float FadeDuration = FMath::IsFinite(FlyingSoundFadeOutDuration)
		? FMath::Max(0.0f, FlyingSoundFadeOutDuration)
		: 0.0f;
	if (bFadeOut && FadeDuration > UE_SMALL_NUMBER
		&& ActiveFlyingAudio->IsPlaying())
	{
		ActiveFlyingAudio->FadeOut(FadeDuration, 0.0f);
	}
	else
	{
		ActiveFlyingAudio->Stop();
	}
	ActiveFlyingAudio = nullptr;
}

FVector UNPThrowableRelicComponent::CalculateThrowVelocity(
	const FVector& ForwardDirection,
	const FVector& ThrowerVelocity,
	const FNPRelicThrowSettings& Settings)
{
	FVector Direction = ForwardDirection.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		Direction = FVector::ForwardVector;
	}
	const FVector InheritedVelocity = Settings.bInheritThrowerVelocity
		&& !ThrowerVelocity.ContainsNaN()
		? ThrowerVelocity
		: FVector::ZeroVector;
	const float ForwardSpeed = FMath::IsFinite(Settings.ForwardSpeed)
		? FMath::Max(0.0f, Settings.ForwardSpeed) : 0.0f;
	const float UpwardSpeed = FMath::IsFinite(Settings.UpwardSpeed)
		? FMath::Max(0.0f, Settings.UpwardSpeed) : 0.0f;
	return Direction * ForwardSpeed
		+ FVector::UpVector * UpwardSpeed
		+ InheritedVelocity;
}

FVector UNPThrowableRelicComponent::CalculateAngularVelocityDegrees(
	const FTransform& RelicTransform,
	const FNPRelicThrowSettings& Settings)
{
	FVector LocalAxis = Settings.LocalSpinAxis.GetSafeNormal();
	if (LocalAxis.IsNearlyZero())
	{
		LocalAxis = FVector::YAxisVector;
	}
	const float SpinSpeed = FMath::IsFinite(Settings.SpinSpeed)
		? FMath::Max(0.0f, Settings.SpinSpeed) : 0.0f;
	return RelicTransform.TransformVectorNoScale(LocalAxis).GetSafeNormal()
		* SpinSpeed;
}

UPrimitiveComponent* UNPThrowableRelicComponent::ResolveRelicMesh() const
{
	return GetOwner()
		? Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent())
		: nullptr;
}

void UNPThrowableRelicComponent::MulticastBeginPawnCollisionGrace_Implementation(
	const float Duration)
{
	UWorld* World = GetWorld();
	UPrimitiveComponent* Mesh = ResolveRelicMesh();
	if (!IsValid(World) || !IsValid(Mesh))
	{
		return;
	}
	RestorePawnCollision();
	CollisionGraceMesh = Mesh;
	const FNPRelicThrowSettings& Settings = GetThrowSettings();
	if (Settings.bIgnorePawnDuringCollisionGrace)
	{
		PreviousPawnCollisionResponse = Mesh->GetCollisionResponseToChannel(ECC_Pawn);
		Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		bPawnCollisionGraceApplied = true;
	}
	if (Settings.bIgnorePhysicsBodyDuringCollisionGrace)
	{
		PreviousPhysicsBodyCollisionResponse =
			Mesh->GetCollisionResponseToChannel(ECC_PhysicsBody);
		Mesh->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);
		bPhysicsBodyCollisionGraceApplied = true;
	}
	if (!bPawnCollisionGraceApplied && !bPhysicsBodyCollisionGraceApplied)
	{
		CollisionGraceMesh.Reset();
		return;
	}
	World->GetTimerManager().SetTimer(
		CollisionGraceTimer,
		this,
		&ThisClass::RestorePawnCollision,
		FMath::Max(Duration, 0.01f),
		false);
}

void UNPThrowableRelicComponent::RestorePawnCollision()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CollisionGraceTimer);
	}
	if (UPrimitiveComponent* Mesh = CollisionGraceMesh.Get())
	{
		if (bPawnCollisionGraceApplied)
		{
			Mesh->SetCollisionResponseToChannel(ECC_Pawn, PreviousPawnCollisionResponse);
		}
		if (bPhysicsBodyCollisionGraceApplied)
		{
			Mesh->SetCollisionResponseToChannel(
				ECC_PhysicsBody,
				PreviousPhysicsBodyCollisionResponse);
		}
	}
	CollisionGraceMesh.Reset();
	bPawnCollisionGraceApplied = false;
	bPhysicsBodyCollisionGraceApplied = false;
}

void UNPThrowableRelicComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FlightTimeoutTimer);
	}
	if (UPrimitiveComponent* HitMesh = FlightHitMesh.Get())
	{
		HitMesh->OnComponentHit.RemoveDynamic(
			this,
			&ThisClass::HandleThrownRelicHit);
		HitMesh->SetNotifyRigidBodyCollision(
			bPreviousFlightNotifyRigidBodyCollision);
	}
	FlightHitMesh.Reset();
	bFlightActive = false;
	bFirstImpactPresented = false;
	ThrowInstigator.Reset();
	ThrowSourceAbilitySystem.Reset();
	StopFlyingAudio(false);
	if (UGrabbableComponent* Grabbable = GetOwner()
		? GetOwner()->FindComponentByClass<UGrabbableComponent>()
		: nullptr)
	{
		Grabbable->OnGrabStarted.RemoveAll(this);
	}
	RestorePawnCollision();
	Super::EndPlay(EndPlayReason);
}
