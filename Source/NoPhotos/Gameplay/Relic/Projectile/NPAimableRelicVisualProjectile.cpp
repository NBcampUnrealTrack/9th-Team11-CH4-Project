#include "Gameplay/Relic/Projectile/NPAimableRelicVisualProjectile.h"

#include "Components/SceneComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NoPhotos.h"
#include "TimerManager.h"

ANPAimableRelicVisualProjectile::ANPAimableRelicVisualProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	TrailEffectComponent = CreateDefaultSubobject<UNiagaraComponent>(
		TEXT("TrailEffect"));
	TrailEffectComponent->SetupAttachment(SceneRoot);
	TrailEffectComponent->SetAutoActivate(false);
	TrailEffectComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TrailEffectComponent->SetGenerateOverlapEvents(false);
	TrailEffectComponent->SetCanEverAffectNavigation(false);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(
		TEXT("ProjectileMovement"));
	ProjectileMovement->SetUpdatedComponent(SceneRoot);
	ProjectileMovement->bAutoActivate = false;
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->bSweepCollision = false;
}

void ANPAimableRelicVisualProjectile::OnConstruction(
	const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	TrailEffectComponent->SetAsset(TrailEffect);
	TrailEffectComponent->SetRelativeRotation(TrailRelativeRotation);
	TrailEffectComponent->SetRelativeScale3D(FVector(FMath::Max(0.0f, TrailScale)));
}

void ANPAimableRelicVisualProjectile::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ArrivalTimer);
	}

	Super::EndPlay(EndPlayReason);
}

void ANPAimableRelicVisualProjectile::InitializeVisualProjectile(
	const FVector& StartLocation,
	const FVector& EndLocation)
{
	if (StartLocation.ContainsNaN() || EndLocation.ContainsNaN())
	{
		UE_LOG(
			LogNoPhotos,
			Error,
			TEXT("[VisualProjectile][Initialize] Rejected: invalid location. Actor=%s Start=%s End=%s"),
			*GetNameSafe(this),
			*StartLocation.ToCompactString(),
			*EndLocation.ToCompactString());
		Destroy();
		return;
	}

	const FVector TravelOffset = EndLocation - StartLocation;
	const float TravelDistance = TravelOffset.Size();
	const FVector TravelDirection = TravelOffset.GetSafeNormal();
	UE_LOG(
		LogNoPhotos,
		Warning,
		TEXT("[VisualProjectile][Initialize] Actor=%s Trail=%s NiagaraComponent=%s Start=%s End=%s Distance=%.1f"),
		*GetNameSafe(this),
		*GetNameSafe(TrailEffect),
		*GetNameSafe(TrailEffectComponent),
		*StartLocation.ToCompactString(),
		*EndLocation.ToCompactString(),
		TravelDistance);
	if (TravelDistance <= UE_SMALL_NUMBER || TravelDirection.IsNearlyZero())
	{
		UE_LOG(
			LogNoPhotos,
			Error,
			TEXT("[VisualProjectile][Initialize] Rejected: zero travel distance or direction. Actor=%s Distance=%.3f Direction=%s"),
			*GetNameSafe(this),
			TravelDistance,
			*TravelDirection.ToCompactString());
		Destroy();
		return;
	}

	SetActorLocationAndRotation(
		StartLocation,
		TravelDirection.Rotation(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	DestinationLocation = EndLocation;

	const float SafeTravelSpeed = FMath::Max(1.0f, TravelSpeed);
	const float NaturalTravelTime = TravelDistance / SafeTravelSpeed;
	const float TravelTime = FMath::Clamp(
		NaturalTravelTime,
		FMath::Max(0.0f, MinimumVisibleDuration),
		FMath::Max(0.01f, MaximumLifeTime));
	const float EffectiveSpeed = TravelDistance / FMath::Max(TravelTime, UE_SMALL_NUMBER);

	ProjectileMovement->InitialSpeed = EffectiveSpeed;
	ProjectileMovement->MaxSpeed = EffectiveSpeed;
	ProjectileMovement->Velocity = TravelDirection * EffectiveSpeed;
	ProjectileMovement->Activate(true);

	if (IsValid(TrailEffect))
	{
		TrailEffectComponent->SetAsset(TrailEffect);
		TrailEffectComponent->Activate(true);
	}
	else
	{
		UE_LOG(
			LogNoPhotos,
			Error,
			TEXT("[VisualProjectile][Initialize] TrailEffect is not assigned. Actor=%s Class=%s"),
			*GetNameSafe(this),
			*GetNameSafe(GetClass()));
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ArrivalTimer,
			this,
			&ThisClass::HandleReachedDestination,
			TravelTime,
			false);
	}
	else
	{
		Destroy();
		return;
	}
	UE_LOG(
		LogNoPhotos,
		Warning,
		TEXT("[VisualProjectile][Initialize] Movement applied. Actor=%s Speed=%.1f TravelTime=%.3f MovementActive=%s NiagaraAsset=%s NiagaraActive=%s Location=%s"),
		*GetNameSafe(this),
		EffectiveSpeed,
		TravelTime,
		ProjectileMovement->IsActive() ? TEXT("true") : TEXT("false"),
		*GetNameSafe(TrailEffectComponent->GetAsset()),
		TrailEffectComponent->IsActive() ? TEXT("true") : TEXT("false"),
		*GetActorLocation().ToCompactString());
}

void ANPAimableRelicVisualProjectile::HandleReachedDestination()
{
	SetActorLocation(
		DestinationLocation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	ProjectileMovement->StopMovementImmediately();
	ProjectileMovement->Deactivate();

	const float SafePostArrivalLifeTime = FMath::Max(
		0.0f,
		PostArrivalLifeTime);
	UE_LOG(
		LogNoPhotos,
		Warning,
		TEXT("[VisualProjectile][Arrival] Actor=%s Destination=%s HoldTime=%.2f NiagaraActive=%s"),
		*GetNameSafe(this),
		*DestinationLocation.ToCompactString(),
		SafePostArrivalLifeTime,
		TrailEffectComponent->IsActive() ? TEXT("true") : TEXT("false"));

	if (SafePostArrivalLifeTime <= UE_SMALL_NUMBER)
	{
		Destroy();
		return;
	}

	SetLifeSpan(SafePostArrivalLifeTime);
}
