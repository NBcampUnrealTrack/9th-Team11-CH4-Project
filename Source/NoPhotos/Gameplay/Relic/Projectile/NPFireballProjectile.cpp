#include "Gameplay/Relic/Projectile/NPFireballProjectile.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/AudioComponent.h"
#include "Components/SphereComponent.h"
#include "Core/GameplayTag/NPGameplayTags.h"
#include "DrawDebugHelpers.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Gameplay/Character/NPStablePhysicsPawn.h"
#include "Gameplay/Relic/Case/NPRelicCase.h"
#include "Gameplay/Relic/Components/NPImpactReceiveComponent.h"
#include "Gameplay/Relic/NPBaseRelic.h"
#include "GameplayEffect.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

ANPFireballProjectile::ANPFireballProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(
		TEXT("CollisionComponent"));
	SetRootComponent(CollisionComponent);
	CollisionComponent->InitSphereRadius(15.0f);
	CollisionComponent->SetCollisionProfileName(
		UCollisionProfile::BlockAllDynamic_ProfileName);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CollisionComponent->SetNotifyRigidBodyCollision(true);
	CollisionComponent->OnComponentHit.AddDynamic(
		this,
		&ANPFireballProjectile::HandleProjectileHit);

	FireballEffectComponent = CreateDefaultSubobject<UNiagaraComponent>(
		TEXT("FireballEffectComponent"));
	FireballEffectComponent->SetupAttachment(CollisionComponent);

	FlightAudioComponent = CreateDefaultSubobject<UAudioComponent>(
		TEXT("FlightAudioComponent"));
	FlightAudioComponent->SetupAttachment(CollisionComponent);
	FlightAudioComponent->SetAutoActivate(false);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(
		TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionComponent;
	ProjectileMovement->InitialSpeed = 1600.0f;
	ProjectileMovement->MaxSpeed = 1600.0f;
	ProjectileMovement->ProjectileGravityScale = 1.0f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> FlightEffectAsset(
		TEXT("/Game/Fire_EXP_Vol01_Free/Niagara/Fire/Loop/NS_Sub_FireTorch_Loop_002.NS_Sub_FireTorch_Loop_002"));
	if (FlightEffectAsset.Succeeded())
	{
		FlightEffect = FlightEffectAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> ExplosionEffectAsset(
		TEXT("/Game/Fire_EXP_Vol01_Free/Niagara/EXP/NS_Sub_EXP_Large_001_01.NS_Sub_EXP_Large_001_01"));
	if (ExplosionEffectAsset.Succeeded())
	{
		ExplosionEffect = ExplosionEffectAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<USoundBase> ExplosionSoundAsset(
		TEXT("/Game/NoPhotos/Resources/Sound/SFX/Rellic/soundreality-explosion-fx-343683.soundreality-explosion-fx-343683"));
	if (ExplosionSoundAsset.Succeeded())
	{
		ExplosionSound = ExplosionSoundAsset.Object;
	}
}

void ANPFireballProjectile::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	FireballEffectComponent->SetAsset(FlightEffect);
	FireballEffectComponent->SetRelativeLocation(
		FlightEffectRelativeLocation);
	FireballEffectComponent->SetRelativeRotation(
		FlightEffectRelativeRotation);
	FireballEffectComponent->SetVariableFloat(
		TEXT("User.FireballScale"),
		FMath::Max(FireballScale, 0.0f));
	FlightAudioComponent->SetSound(FlightSound);
	FlightAudioComponent->SetVolumeMultiplier(
		FMath::Max(FlightSoundVolume, 0.0f));
}

void ANPFireballProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (IsValid(FlightSound))
	{
		FlightAudioComponent->Play();
	}
}

void ANPFireballProjectile::InitializeProjectile(
	UAbilitySystemComponent* InSourceAbilitySystem,
	ANPBaseRelic* InSourceRelic,
	const FVector& LaunchVelocity,
	const FNPFireballExplosionSettings& InExplosionSettings,
	const float LifeTime)
{
	if (!HasAuthority())
	{
		return;
	}

	SourceAbilitySystem = InSourceAbilitySystem;
	SourceRelic = InSourceRelic;
	ExplosionSettings = InExplosionSettings;

	CollisionComponent->IgnoreActorWhenMoving(GetInstigator(), true);
	CollisionComponent->IgnoreActorWhenMoving(SourceRelic, true);
	ProjectileMovement->InitialSpeed = LaunchVelocity.Size();
	ProjectileMovement->MaxSpeed = LaunchVelocity.Size();
	ProjectileMovement->Velocity = LaunchVelocity;
	bInitialized = true;
	CollisionComponent->SetCollisionEnabled(
		ECollisionEnabled::QueryOnly);
	SetLifeSpan(FMath::Max(LifeTime, 0.1f));
}

void ANPFireballProjectile::HandleProjectileHit(
	UPrimitiveComponent*,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	FVector,
	const FHitResult& Hit)
{
	if (!HasAuthority()
		|| !bInitialized
		|| bExploded
		|| OtherActor == GetInstigator()
		|| OtherActor == SourceRelic
		|| OtherActor == GetOwner())
	{
		return;
	}
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			INDEX_NONE,
			5.0f,
			FColor::Orange,
			TEXT("[Fireball 3/4] 투사체 충돌: 폭발 실행"));
		GEngine->AddOnScreenDebugMessage(
			INDEX_NONE,
			5.0f,
			FColor::Yellow,
			FString::Printf(
				TEXT("[Fireball 4/4] 충돌 대상: %s / %s / %s"),
				*GetNameSafe(OtherActor),
				OtherActor
					? *GetNameSafe(OtherActor->GetClass())
					: TEXT("None"),
				*GetNameSafe(OtherComponent)));
	}

	const FVector ExplosionLocation = Hit.bBlockingHit
		? FVector(Hit.ImpactPoint)
		: GetActorLocation();
	Explode(ExplosionLocation);
}

void ANPFireballProjectile::Explode(const FVector& ExplosionLocation)
{
	bExploded = true;
	ProjectileMovement->StopMovementImmediately();
	ProjectileMovement->Deactivate();
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ApplyExplosionImpulse(ExplosionLocation);
	MulticastExplode(
		ExplosionLocation,
		FMath::Max(ExplosionSettings.Radius, 1.0f));
	SetLifeSpan(0.2f);
}

void ANPFireballProjectile::ApplyExplosionImpulse(
	const FVector& ExplosionLocation)
{
	UWorld* World = GetWorld();
	const float Radius = FMath::Max(ExplosionSettings.Radius, 1.0f);
	if (!IsValid(World))
	{
		return;
	}

	const FCollisionObjectQueryParams ObjectQuery(
		FCollisionObjectQueryParams::AllObjects);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FireballExplosion), false);
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(SourceRelic);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(
		Overlaps,
		ExplosionLocation,
		FQuat::Identity,
		ObjectQuery,
		FCollisionShape::MakeSphere(Radius),
		QueryParams);

	TSet<TWeakObjectPtr<AActor>> AffectedActors;
	TSet<TWeakObjectPtr<UNPImpactReceiveComponent>> DepletedDurabilityComponents;
	TSet<TWeakObjectPtr<UPrimitiveComponent>> AffectedPhysicsComponents;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* TargetActor = Overlap.GetActor();
		if (!IsValid(TargetActor)
			|| TargetActor == SourceRelic)
		{
			continue;
		}

		ANPStablePhysicsPawn* TargetPawn = Cast<ANPStablePhysicsPawn>(TargetActor);
		ANPBaseRelic* TargetRelic = Cast<ANPBaseRelic>(TargetActor);
		UPrimitiveComponent* TargetPhysicsComponent = Overlap.GetComponent();
		if (TargetPawn || TargetRelic)
		{
			const TWeakObjectPtr<AActor> TargetKey(TargetActor);
			if (AffectedActors.Contains(TargetKey))
			{
				continue;
			}
			AffectedActors.Add(TargetKey);
		}
		else
		{
			const TWeakObjectPtr<UPrimitiveComponent> ComponentKey(
				TargetPhysicsComponent);
			if (!IsValid(TargetPhysicsComponent)
				|| !TargetPhysicsComponent->IsSimulatingPhysics()
				|| AffectedPhysicsComponents.Contains(ComponentKey))
			{
				continue;
			}
			AffectedPhysicsComponents.Add(ComponentKey);
		}

		const FVector TargetLocation = TargetPhysicsComponent
			&& !TargetPawn
			&& !TargetRelic
			? TargetPhysicsComponent->GetComponentLocation()
			: TargetActor->GetActorLocation();
		if (TargetActor->IsA<ANPRelicCase>())
		{
			UNPImpactReceiveComponent* ImpactReceiveComponent =
				TargetActor->FindComponentByClass<UNPImpactReceiveComponent>();
			const TWeakObjectPtr<UNPImpactReceiveComponent> ComponentKey(
				ImpactReceiveComponent);
			if (IsValid(ImpactReceiveComponent)
				&& !DepletedDurabilityComponents.Contains(ComponentKey))
			{
				DepletedDurabilityComponents.Add(ComponentKey);
				ImpactReceiveComponent->DepleteDurability(ExplosionLocation);
			}
		}

		FVector HorizontalDirection = TargetLocation - ExplosionLocation;
		HorizontalDirection.Z = 0.0f;
		if (!HorizontalDirection.Normalize())
		{
			HorizontalDirection = FVector::ForwardVector;
		}

		const float Distance = FVector::Distance(
			ExplosionLocation,
			TargetLocation);
		const float StrengthAlpha = 1.0f - FMath::Clamp(
			Distance / Radius,
			0.0f,
			1.0f);
		const FVector KnockbackVelocity =
			HorizontalDirection
				* FMath::Max(
					ExplosionSettings.HorizontalKnockbackStrength,
					0.0f)
				* StrengthAlpha
			+ FVector::UpVector
				* FMath::Max(
					ExplosionSettings.UpwardKnockbackStrength,
					0.0f)
				* StrengthAlpha;
		if (KnockbackVelocity.IsNearlyZero())
		{
			continue;
		}

		if (TargetPawn)
		{
			ApplyCharacterKnockback(TargetActor, KnockbackVelocity);
		}
		else if (TargetRelic)
		{
			TargetRelic->ReleaseWithVelocityImpulse(KnockbackVelocity);
		}
		else
		{
			TargetPhysicsComponent->AddImpulse(
				KnockbackVelocity,
				NAME_None,
				true);
		}
	}
}

void ANPFireballProjectile::ApplyCharacterKnockback(
	AActor* TargetActor,
	const FVector& KnockbackVelocity)
{
	if (!IsValid(SourceAbilitySystem)
		|| !ExplosionSettings.KnockbackEffectClass)
	{
		return;
	}

	UAbilitySystemComponent* TargetAbilitySystem =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	if (!IsValid(TargetAbilitySystem))
	{
		return;
	}

	FHitResult KnockbackHit;
	KnockbackHit.TraceStart = GetActorLocation();
	KnockbackHit.TraceEnd = KnockbackHit.TraceStart
		+ KnockbackVelocity.GetSafeNormal();
	FGameplayEffectContextHandle EffectContext =
		SourceAbilitySystem->MakeEffectContext();
	EffectContext.AddSourceObject(SourceRelic);
	EffectContext.AddHitResult(KnockbackHit, true);

	FGameplayEffectSpecHandle EffectSpec = SourceAbilitySystem->MakeOutgoingSpec(
		ExplosionSettings.KnockbackEffectClass,
		1.0f,
		EffectContext);
	if (!EffectSpec.IsValid())
	{
		return;
	}

	EffectSpec.Data->AddDynamicAssetTag(NPGameplayTags::Effect_Knockback);
	EffectSpec.Data->SetSetByCallerMagnitude(
		NPGameplayTags::Data_Knockback_Magnitude,
		KnockbackVelocity.Size());
	SourceAbilitySystem->ApplyGameplayEffectSpecToTarget(
		*EffectSpec.Data.Get(),
		TargetAbilitySystem);
}

void ANPFireballProjectile::MulticastExplode_Implementation(
	const FVector_NetQuantize10 ExplosionLocation,
	const float ExplosionRadius)
{
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FireballEffectComponent->Deactivate();
	FlightAudioComponent->Stop();
	SetActorHiddenInGame(true);

#if ENABLE_DRAW_DEBUG
	DrawDebugSphere(
		GetWorld(),
		ExplosionLocation,
		FMath::Max(ExplosionRadius, 1.0f),
		32,
		FColor::Red,
		false,
		2.0f,
		0,
		2.0f);
#endif

	if (IsValid(ExplosionEffect))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this,
			ExplosionEffect,
			ExplosionLocation);
	}
	if (IsValid(ExplosionSound))
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			ExplosionSound,
			ExplosionLocation,
			FMath::Max(ExplosionSoundVolume, 0.0f),
			1.0f,
			FMath::Max(ExplosionSoundStartTime, 0.0f));
	}
}
