#include "Gameplay/Relic/Components/NPSwingableRelicComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Core/GameplayTag/NPGameplayTags.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Gameplay/AbilitySystem/Effects/NPKnockbackGameplayEffect.h"
#include "Gameplay/Relic/Abilities/NPRelicUseAbility.h"
#include "GameplayEffect.h"
#include "PhysicsEngine/BodyInstance.h"

UNPSwingableRelicComponent::UNPSwingableRelicComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetUseAbilityClass(UNPRelicUseAbility::StaticClass());
	SwingSettings.KnockbackEffectClass =
		UNPKnockbackGameplayEffect::StaticClass();
}

void UNPSwingableRelicComponent::StartHitDetection(
	AActor* InAttackInstigator,
	UAbilitySystemComponent* InSourceAbilitySystem,
	const FVector& InCameraDirection)
{
	AActor* OwnerActor = GetOwner();
	const FNPRelicSwingSettings& Settings = GetSwingSettings();
	if (bHitDetectionActive
		|| !OwnerActor
		|| !OwnerActor->HasAuthority()
		|| !IsValid(InAttackInstigator)
		|| !IsValid(InSourceAbilitySystem)
		|| !Settings.KnockbackEffectClass)
	{
		return;
	}

	UPrimitiveComponent* HitMesh = ResolveRelicMesh();
	if (!HitMesh)
	{
		return;
	}

	AttackInstigator = InAttackInstigator;
	SourceAbilitySystem = InSourceAbilitySystem;
	CameraDirection = FVector(InCameraDirection.X, InCameraDirection.Y, 0.0f)
		.GetSafeNormal();
	LastHitTimes.Reset();

	const FBodyInstance* BodyInstance = HitMesh->GetBodyInstance();
	bPreviousNotifyRigidBodyCollision = BodyInstance
		? BodyInstance->bNotifyRigidBodyCollision
		: false;
	HitMesh->SetNotifyRigidBodyCollision(true);
	HitMesh->OnComponentHit.AddUniqueDynamic(
		this,
		&UNPSwingableRelicComponent::HandleRelicHit);
	bHitDetectionActive = true;
}

void UNPSwingableRelicComponent::StopHitDetection()
{
	if (!bHitDetectionActive)
	{
		return;
	}

	if (UPrimitiveComponent* HitMesh = ResolveRelicMesh())
	{
		HitMesh->OnComponentHit.RemoveDynamic(
			this,
			&UNPSwingableRelicComponent::HandleRelicHit);
		HitMesh->SetNotifyRigidBodyCollision(
			bPreviousNotifyRigidBodyCollision);
	}

	bHitDetectionActive = false;
	AttackInstigator.Reset();
	SourceAbilitySystem.Reset();
	CameraDirection = FVector::ForwardVector;
	LastHitTimes.Reset();
}

void UNPSwingableRelicComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	StopHitDetection();
	Super::EndPlay(EndPlayReason);
}

void UNPSwingableRelicComponent::HandleRelicHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent*,
	FVector,
	const FHitResult& Hit)
{
	AActor* OwnerActor = GetOwner();
	AActor* InstigatorActor = AttackInstigator.Get();
	UAbilitySystemComponent* SourceASC = SourceAbilitySystem.Get();
	UWorld* World = GetWorld();
	const FNPRelicSwingSettings& Settings = GetSwingSettings();
	if (!bHitDetectionActive
		|| !OwnerActor
		|| !OwnerActor->HasAuthority()
		|| !World
		|| !HitComponent
		|| !IsValid(OtherActor)
		|| OtherActor == OwnerActor
		|| OtherActor == InstigatorActor
		|| !SourceASC
		|| !Settings.KnockbackEffectClass)
	{
		return;
	}

	const TWeakObjectPtr<AActor> TargetKey(OtherActor);
	const double CurrentTime = World->GetTimeSeconds();
	if (const double* LastHitTime = LastHitTimes.Find(TargetKey);
		LastHitTime
		&& CurrentTime - *LastHitTime < Settings.RehitCooldown)
	{
		return;
	}

	const FVector ImpactVelocity =
		HitComponent->GetPhysicsLinearVelocityAtPoint(Hit.ImpactPoint);
	if (FVector(ImpactVelocity.X, ImpactVelocity.Y, 0.0f).Size()
		< Settings.MinimumHitSpeed)
	{
		return;
	}

	const FVector KnockbackDirection = CalculateKnockbackDirection(Hit);
	if (KnockbackDirection.IsNearlyZero())
	{
		return;
	}

	UAbilitySystemComponent* TargetASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor);
	if (!TargetASC)
	{
		return;
	}

	FHitResult KnockbackHit = Hit;
	// Effect Context의 Trace 방향을 넉백 방향 전달에 사용합니다.
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
		return;
	}

	EffectSpec.Data->AddDynamicAssetTag(
		NPGameplayTags::Effect_Knockback);
	EffectSpec.Data->SetSetByCallerMagnitude(
		NPGameplayTags::Data_Knockback_Magnitude,
		Settings.KnockbackStrength);
	SourceASC->ApplyGameplayEffectSpecToTarget(
		*EffectSpec.Data.Get(),
		TargetASC);
	LastHitTimes.FindOrAdd(TargetKey) = CurrentTime;

#if ENABLE_DRAW_DEBUG
	if (Settings.bDrawKnockbackDirection)
	{
		DrawDebugSphere(
			World,
			Hit.ImpactPoint,
			12.0f,
			12,
			FColor::Cyan,
			false,
			2.0f,
			1,
			4.0f);
		DrawDebugDirectionalArrow(
			World,
			Hit.ImpactPoint,
			Hit.ImpactPoint + KnockbackDirection * 300.0f,
			60.0f,
			FColor::Cyan,
			false,
			2.0f,
			1,
			8.0f);
	}
#endif
}

FVector UNPSwingableRelicComponent::CalculateKnockbackDirection(
	const FHitResult& Hit) const
{
	if (!RelicMesh)
	{
		return FVector::ZeroVector;
	}

	FVector PhysicalDirection =
		RelicMesh->GetPhysicsLinearVelocityAtPoint(Hit.ImpactPoint);
	PhysicalDirection.Z = 0.0f;
	PhysicalDirection.Normalize();

	if (CameraDirection.IsNearlyZero())
	{
		return PhysicalDirection;
	}
	if (PhysicalDirection.IsNearlyZero())
	{
		return CameraDirection;
	}

	const float CameraWeight = FMath::Clamp(
		GetSwingSettings().CameraDirectionWeight,
		0.0f,
		1.0f);
	if (CameraWeight <= UE_SMALL_NUMBER)
	{
		return PhysicalDirection;
	}
	if (CameraWeight >= 1.0f - UE_SMALL_NUMBER)
	{
		return CameraDirection;
	}

	const float ForwardAmount = FVector::DotProduct(
		PhysicalDirection,
		CameraDirection);
	if (ForwardAmount < 0.0f)
	{
		PhysicalDirection -= CameraDirection * ForwardAmount;
		PhysicalDirection.Normalize();
	}

	return FMath::Lerp(
		PhysicalDirection,
		CameraDirection,
		CameraWeight).GetSafeNormal();
}

UPrimitiveComponent* UNPSwingableRelicComponent::ResolveRelicMesh()
{
	if (!RelicMesh)
	{
		RelicMesh = GetOwner()
			? Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent())
			: nullptr;
	}
	return RelicMesh;
}
