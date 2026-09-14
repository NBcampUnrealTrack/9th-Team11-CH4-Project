#include "Gameplay/Map/Trap/NPTrapKnockbackComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Core/GameplayTag/NPGameplayTags.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Gameplay/AbilitySystem/Effects/NPKnockbackGameplayEffect.h"
#include "GameplayEffect.h"

UNPTrapKnockbackComponent::UNPTrapKnockbackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	KnockbackEffectClass = UNPKnockbackGameplayEffect::StaticClass();
}

void UNPTrapKnockbackComponent::BeginActivationCycle(
	const int32 CycleSequence)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	ActiveCycleSequence = CycleSequence;
	bActivationCycleActive = true;
	HitPawnsThisCycle.Reset();
	RemoveInvalidTargetRecords();
}

void UNPTrapKnockbackComponent::EndActivationCycle()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	bActivationCycleActive = false;
	HitPawnsThisCycle.Reset();
}

bool UNPTrapKnockbackComponent::TryApplyKnockback(
	AActor* OtherActor,
	const FHitResult& Hit,
	const FVector& WorldKnockbackDirection)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor
		|| !OwnerActor->HasAuthority()
		|| !bActivationCycleActive
		|| !KnockbackEffectClass)
	{
		return false;
	}

	APawn* TargetPawn = Cast<APawn>(OtherActor);
	if (!IsValid(TargetPawn) || TargetPawn == OwnerActor)
	{
		return false;
	}

	const TWeakObjectPtr<APawn> TargetKey(TargetPawn);
	if (bHitOncePerActivation && HitPawnsThisCycle.Contains(TargetKey))
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const double CurrentTime = World->GetTimeSeconds();
	if (const double* LastHitTime = LastHitTimes.Find(TargetKey);
		LastHitTime
		&& CurrentTime - *LastHitTime
			< FMath::Max(0.0f, PerTargetRehitCooldown))
	{
		return false;
	}

	UAbilitySystemComponent* TargetAbilitySystem =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetPawn);
	if (!IsValid(TargetAbilitySystem))
	{
		return false;
	}

	const FVector HorizontalDirection = FVector(
		WorldKnockbackDirection.X,
		WorldKnockbackDirection.Y,
		0.0f).GetSafeNormal();
	if (HorizontalDirection.IsNearlyZero())
	{
		return false;
	}

	const FVector KnockbackVelocity =
		HorizontalDirection * FMath::Max(0.0f, ForwardKnockbackStrength)
		+ FVector::UpVector * FMath::Max(0.0f, UpwardKnockbackStrength);
	const float KnockbackMagnitude = KnockbackVelocity.Size();
	if (KnockbackMagnitude <= UE_SMALL_NUMBER)
	{
		return false;
	}

	FHitResult KnockbackHit = Hit;
	const FVector ImpactLocation = Hit.bBlockingHit
		? FVector(Hit.ImpactPoint)
		: TargetPawn->GetActorLocation();
	KnockbackHit.TraceStart = ImpactLocation;
	KnockbackHit.TraceEnd =
		ImpactLocation + KnockbackVelocity.GetSafeNormal();

	FGameplayEffectContextHandle EffectContext =
		TargetAbilitySystem->MakeEffectContext();
	EffectContext.AddInstigator(OwnerActor, OwnerActor);
	EffectContext.AddSourceObject(OwnerActor);
	EffectContext.AddHitResult(KnockbackHit, true);

	const UGameplayEffect* EffectDefinition =
		KnockbackEffectClass->GetDefaultObject<UGameplayEffect>();
	if (!EffectDefinition)
	{
		return false;
	}

	FGameplayEffectSpec EffectSpec(
		EffectDefinition,
		EffectContext,
		1.0f);
	EffectSpec.AddDynamicAssetTag(NPGameplayTags::Effect_Knockback);
	EffectSpec.SetSetByCallerMagnitude(
		NPGameplayTags::Data_Knockback_Magnitude,
		KnockbackMagnitude);

	const FActiveGameplayEffectHandle AppliedHandle =
		TargetAbilitySystem->ApplyGameplayEffectSpecToSelf(EffectSpec);
	if (!AppliedHandle.IsValid())
	{
		return false;
	}

	HitPawnsThisCycle.Add(TargetKey);
	LastHitTimes.FindOrAdd(TargetKey) = CurrentTime;
	return true;
}

void UNPTrapKnockbackComponent::RemoveInvalidTargetRecords()
{
	for (auto It = LastHitTimes.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}
