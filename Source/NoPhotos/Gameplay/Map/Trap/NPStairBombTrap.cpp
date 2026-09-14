#include "Gameplay/Map/Trap/NPStairBombTrap.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/GameplayTag/NPGameplayTags.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Gameplay/AbilitySystem/Effects/NPKnockbackGameplayEffect.h"
#include "GameplayEffect.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogNPStairBombTrap, Log, All);

ANPStairBombTrap::ANPStairBombTrap()
{
	OperationMode = ENPStairTrapOperationMode::Cyclic;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	ThrowOriginComponent = CreateDefaultSubobject<USceneComponent>(
		TEXT("ThrowOriginComponent"));
	ThrowOriginComponent->SetupAttachment(TrapRootComponent);

	ThrowTargetComponent = CreateDefaultSubobject<USceneComponent>(
		TEXT("ThrowTargetComponent"));
	ThrowTargetComponent->SetupAttachment(TrapRootComponent);
	ThrowTargetComponent->SetRelativeLocation(FVector(500.0f, 0.0f, 0.0f));

	BombMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("BombMeshComponent"));
	BombMeshComponent->SetupAttachment(TrapRootComponent);
	BombMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BombMeshComponent->SetGenerateOverlapEvents(false);
	BombMeshComponent->SetHiddenInGame(true);

	ExplosionAbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(
		TEXT("ExplosionAbilitySystem"));
	ExplosionAbilitySystem->SetIsReplicated(false);
	KnockbackEffectClass = UNPKnockbackGameplayEffect::StaticClass();
}

void ANPStairBombTrap::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && IsValid(ExplosionAbilitySystem))
	{
		ExplosionAbilitySystem->InitAbilityActorInfo(this, this);
	}

	SetBombVisible(false);
}

void ANPStairBombTrap::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateBombPose();
}

UAbilitySystemComponent* ANPStairBombTrap::GetAbilitySystemComponent() const
{
	return ExplosionAbilitySystem;
}

void ANPStairBombTrap::HandleTrapStateChanged(
	const ENPStairTrapState PreviousState,
	const ENPStairTrapState NewState)
{
	Super::HandleTrapStateChanged(PreviousState, NewState);

	switch (NewState)
	{
	case ENPStairTrapState::Warning:
		BeginThrow();
		break;
	case ENPStairTrapState::Active:
		UpdateBombPose();
		ExplodeOnce();
		break;
	case ENPStairTrapState::Returning:
		SetActorTickEnabled(false);
		SetBombVisible(false);
		BP_OnThrowerReset();
		break;
	case ENPStairTrapState::Cooldown:
	case ENPStairTrapState::Idle:
	case ENPStairTrapState::Disabled:
	default:
		SetActorTickEnabled(false);
		SetBombVisible(false);
		break;
	}
}

void ANPStairBombTrap::BeginThrow()
{
	ThrowStartLocation = ResolveThrowStartLocation();
	ThrowTargetLocation = ResolveThrowTargetLocation();
	ThrowStartRotation = IsValid(BombMeshComponent)
		? BombMeshComponent->GetComponentRotation()
		: FRotator::ZeroRotator;

	if (IsValid(BombMeshComponent))
	{
		BombMeshComponent->SetWorldLocation(ThrowStartLocation);
	}
	SetBombVisible(true);
	SetActorTickEnabled(true);
	UpdateBombPose();
	BP_OnThrowStarted();
}

void ANPStairBombTrap::UpdateBombPose()
{
	if (GetTrapState() != ENPStairTrapState::Warning
		|| !IsValid(BombMeshComponent))
	{
		return;
	}

	const float SafeDuration = FMath::Max(ThrowDuration, UE_SMALL_NUMBER);
	const float ElapsedTime = GetTrapPhaseElapsedTime();
	const float Alpha = FMath::Clamp(ElapsedTime / SafeDuration, 0.0f, 1.0f);
	const FVector LinearPosition = FMath::Lerp(
		ThrowStartLocation,
		ThrowTargetLocation,
		Alpha);
	const float HeightOffset = 4.0f * FMath::Max(0.0f, ThrowArcHeight)
		* Alpha * (1.0f - Alpha);
	BombMeshComponent->SetWorldLocation(
		LinearPosition + FVector::UpVector * HeightOffset);

	if (bRotateDuringFlight)
	{
		BombMeshComponent->SetWorldRotation(
			ThrowStartRotation + FlightRotationSpeed * ElapsedTime);
	}
}

void ANPStairBombTrap::ExplodeOnce()
{
	const int32 CycleSequence = GetTrapCycleSequence();
	if (!HasAuthority()
		|| LastExplodedCycleSequence == CycleSequence)
	{
		return;
	}

	LastExplodedCycleSequence = CycleSequence;
	const FVector ExplosionLocation = ResolveThrowTargetLocation();
	SetActorTickEnabled(false);
	SetBombVisible(false);
	ApplyBlastKnockback(ExplosionLocation);
	MulticastPlayExplosion(ExplosionLocation, CycleSequence);
}

void ANPStairBombTrap::ApplyBlastKnockback(
	const FVector& ExplosionLocation)
{
	if (!HasAuthority() || !GetWorld() || !KnockbackEffectClass
		|| !IsValid(ExplosionAbilitySystem))
	{
		return;
	}

	int32 AffectedPawnCount = 0;
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		APawn* TargetPawn = *It;
		if (!IsValid(TargetPawn) || TargetPawn->IsActorBeingDestroyed())
		{
			continue;
		}

		const FVector TargetLocation = TargetPawn->GetActorLocation();
		if (FVector::DistSquared(ExplosionLocation, TargetLocation)
			> FMath::Square(static_cast<double>(FMath::Max(0.0f, ExplosionRadius))))
		{
			continue;
		}

		if (bRequireLineOfSight)
		{
			FCollisionQueryParams Query(
				SCENE_QUERY_STAT(StairBombVisibility),
				false,
				this);
			Query.AddIgnoredActor(TargetPawn);
			FHitResult Obstruction;
			if (GetWorld()->LineTraceSingleByChannel(
				Obstruction,
				ExplosionLocation,
				TargetLocation,
				ECC_Visibility,
				Query))
			{
				continue;
			}
		}

		UAbilitySystemComponent* TargetAbilitySystem =
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetPawn);
		if (!IsValid(TargetAbilitySystem))
		{
			continue;
		}

		FVector HorizontalDirection =
			(TargetLocation - ExplosionLocation).GetSafeNormal2D();
		if (HorizontalDirection.IsNearlyZero())
		{
			HorizontalDirection = GetActorForwardVector().GetSafeNormal2D();
		}
		const FVector KnockbackVelocity =
			HorizontalDirection * FMath::Max(0.0f, HorizontalKnockbackStrength)
			+ FVector::UpVector * FMath::Max(0.0f, UpwardKnockbackStrength);
		if (KnockbackVelocity.IsNearlyZero())
		{
			continue;
		}

		FHitResult KnockbackHit;
		KnockbackHit.ImpactPoint = TargetLocation;
		KnockbackHit.TraceStart = ExplosionLocation;
		KnockbackHit.TraceEnd =
			ExplosionLocation + KnockbackVelocity.GetSafeNormal();
		FGameplayEffectContextHandle EffectContext =
			ExplosionAbilitySystem->MakeEffectContext();
		EffectContext.AddInstigator(this, this);
		EffectContext.AddSourceObject(this);
		EffectContext.AddHitResult(KnockbackHit, true);

		FGameplayEffectSpecHandle EffectSpec =
			ExplosionAbilitySystem->MakeOutgoingSpec(
				KnockbackEffectClass,
				1.0f,
				EffectContext);
		if (!EffectSpec.IsValid())
		{
			continue;
		}

		EffectSpec.Data->AddDynamicAssetTag(NPGameplayTags::Effect_Knockback);
		EffectSpec.Data->SetSetByCallerMagnitude(
			NPGameplayTags::Data_Knockback_Magnitude,
			KnockbackVelocity.Size());
		ExplosionAbilitySystem->ApplyGameplayEffectSpecToTarget(
			*EffectSpec.Data.Get(),
			TargetAbilitySystem);
		++AffectedPawnCount;
	}

	UE_LOG(
		LogNPStairBombTrap,
		Log,
		TEXT("Explosion applied. Trap=%s Cycle=%d Location=%s Radius=%.1f Pawns=%d"),
		*GetNameSafe(this),
		GetTrapCycleSequence(),
		*ExplosionLocation.ToCompactString(),
		ExplosionRadius,
		AffectedPawnCount);
}

void ANPStairBombTrap::SetBombVisible(const bool bVisible)
{
	if (IsValid(BombMeshComponent))
	{
		BombMeshComponent->SetHiddenInGame(!bVisible);
		BombMeshComponent->SetVisibility(bVisible, true);
	}
}

FVector ANPStairBombTrap::ResolveThrowStartLocation() const
{
	if (IsValid(ThrowerActor))
	{
		if (!ThrowSocketName.IsNone())
		{
			TArray<UMeshComponent*> MeshComponents;
			ThrowerActor->GetComponents<UMeshComponent>(MeshComponents);
			for (const UMeshComponent* MeshComponent : MeshComponents)
			{
				if (IsValid(MeshComponent)
					&& MeshComponent->DoesSocketExist(ThrowSocketName))
				{
					return MeshComponent->GetSocketLocation(ThrowSocketName);
				}
			}
		}
		return ThrowerActor->GetActorLocation();
	}

	return IsValid(ThrowOriginComponent)
		? ThrowOriginComponent->GetComponentLocation()
		: GetActorLocation();
}

FVector ANPStairBombTrap::ResolveThrowTargetLocation() const
{
	if (IsValid(ThrowTargetActor))
	{
		return ThrowTargetActor->GetActorLocation();
	}

	return IsValid(ThrowTargetComponent)
		? ThrowTargetComponent->GetComponentLocation()
		: GetActorLocation();
}

void ANPStairBombTrap::MulticastPlayExplosion_Implementation(
	const FVector_NetQuantize ExplosionLocation,
	const int32 CycleSequence)
{
	SetActorTickEnabled(false);
	SetBombVisible(false);
	if (GetNetMode() == NM_DedicatedServer
		|| LastPresentedExplosionCycle == CycleSequence)
	{
		return;
	}

	LastPresentedExplosionCycle = CycleSequence;
	if (ExplosionSystem)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this,
			ExplosionSystem,
			ExplosionLocation);
	}
	if (ExplosionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			ExplosionSound,
			ExplosionLocation);
	}
	BP_OnBombExploded(ExplosionLocation);
}
