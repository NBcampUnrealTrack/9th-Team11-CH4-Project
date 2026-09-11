#include "Gameplay/Relic/Components/NPImpactReceiveComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Gameplay/Character/NPStablePhysicsPawn.h"

UNPImpactReceiveComponent::UNPImpactReceiveComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UNPImpactReceiveComponent::BeginPlay()
{
	Super::BeginPlay();

	MaxHealth = FMath::Max(1, MaxHealth);
	CurrentHealth = MaxHealth;

	if (ImpactTargetComponents.IsEmpty())
	{
		AActor* Owner = GetOwner();
		if (UPrimitiveComponent* RootPrimitive = Owner
			? Cast<UPrimitiveComponent>(Owner->GetRootComponent())
			: nullptr)
		{
			ImpactTargetComponents.Add(RootPrimitive);
		}
	}

	for (UPrimitiveComponent* ImpactTargetComponent : ImpactTargetComponents)
	{
		if (IsValid(ImpactTargetComponent))
		{
			ImpactTargetComponent->OnComponentHit.AddUniqueDynamic(
				this,
				&UNPImpactReceiveComponent::HandleHit);
		}
	}
}

void UNPImpactReceiveComponent::SetImpactTargetComponent(
	UPrimitiveComponent* InTargetComponent)
{
	ImpactTargetComponents.Reset();
	if (IsValid(InTargetComponent))
	{
		ImpactTargetComponents.Add(InTargetComponent);
	}
}

void UNPImpactReceiveComponent::SetImpactTargetComponents(
	const TArray<UPrimitiveComponent*>& InTargetComponents)
{
	ImpactTargetComponents.Reset(InTargetComponents.Num());
	for (UPrimitiveComponent* ImpactTargetComponent : InTargetComponents)
	{
		if (IsValid(ImpactTargetComponent))
		{
			ImpactTargetComponents.AddUnique(ImpactTargetComponent);
		}
	}
}

void UNPImpactReceiveComponent::IgnoreGrabImpact()
{
	if (const UWorld* World = GetWorld())
	{
		IgnoreDamageUntilTime = FMath::Max(
			IgnoreDamageUntilTime,
			World->GetTimeSeconds()
				+ FMath::Max(0.0f, GrabImpactIgnoreDuration));
	}
}

void UNPImpactReceiveComponent::SetImpactThresholds(
	const float InMinThreshold,
	const float InMaxThreshold)
{
	MinImpactThreshold = FMath::Max(0.0f, InMinThreshold);
	MaxImpactThreshold = FMath::Max(0.0f, InMaxThreshold);
}

void UNPImpactReceiveComponent::HandleHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	const FVector NormalImpulse,
	const FHitResult& Hit)
{
	AActor* ImpactSource = OtherActor;
	if (!ImpactSource && OtherComponent)
	{
		ImpactSource = OtherComponent->GetOwner();
	}
	const FVector ImpactLocation = Hit.ImpactPoint.IsNearlyZero()
		? HitComponent->GetComponentLocation()
		: FVector(Hit.ImpactPoint);
	ApplyImpact(
		HitComponent,
		ImpactSource,
		NormalImpulse.Size(),
		ImpactLocation);
}

bool UNPImpactReceiveComponent::ApplyExternalImpact(
	UPrimitiveComponent* HitComponent,
	AActor* ImpactSource,
	const float ImpactStrength,
	const FVector& ImpactLocation)
{
	return ApplyImpact(
		HitComponent,
		ImpactSource,
		FMath::Max(ImpactStrength, 0.0f),
		ImpactLocation);
}

bool UNPImpactReceiveComponent::ApplyImpact(
	UPrimitiveComponent* HitComponent,
	AActor* ImpactSource,
	const float ImpactStrength,
	const FVector& ImpactLocation)
{
	AActor* Owner = GetOwner();
	if (!Owner
		|| !Owner->HasAuthority()
		|| CurrentHealth <= 0
		|| !IsValid(HitComponent)
		|| !ImpactTargetComponents.Contains(HitComponent))
	{
		return false;
	}
	if (ImpactSource == Owner)
	{
		return false;
	}
	if (ImpactSource && ImpactSource->IsA<ANPStablePhysicsPawn>())
	{
		return false;
	}

	const UWorld* World = GetWorld();
	const double CurrentTime = World ? World->GetTimeSeconds() : 0.0;
	if (CurrentTime < IgnoreDamageUntilTime
		|| CurrentTime < NextDamageAllowedTime)
	{
		return false;
	}

	const float ValidMinImpact = FMath::Min(
		MinImpactThreshold,
		MaxImpactThreshold);
	const float ValidMaxImpact = FMath::Max(
		MinImpactThreshold,
		MaxImpactThreshold);
	if (ImpactStrength < ValidMinImpact)
	{
		return false;
	}

	const int32 ValidMinDamage = FMath::Max(
		1,
		FMath::Min(MinDamage, MaxDamage));
	const int32 ValidMaxDamage = FMath::Max(
		ValidMinDamage,
		FMath::Max(MinDamage, MaxDamage));
	const float ImpactAlpha = ValidMaxImpact > ValidMinImpact
		? FMath::Clamp(
			(ImpactStrength - ValidMinImpact)
				/ (ValidMaxImpact - ValidMinImpact),
			0.0f,
			1.0f)
		: 1.0f;
	const int32 Damage = FMath::RoundToInt(FMath::Lerp(
		static_cast<float>(ValidMinDamage),
		static_cast<float>(ValidMaxDamage),
		ImpactAlpha));

	NextDamageAllowedTime = CurrentTime + FMath::Max(0.0f, DamageCooldown);
	CurrentHealth = FMath::Max(0, CurrentHealth - Damage);
	OnDamaged.Broadcast(Damage, CurrentHealth, MaxHealth);

	if (CurrentHealth <= 0)
	{
		const FVector ValidImpactLocation = ImpactLocation.IsNearlyZero()
			? HitComponent->GetComponentLocation()
			: ImpactLocation;
		OnDepleted.Broadcast(ValidImpactLocation);
	}

	return true;
}
