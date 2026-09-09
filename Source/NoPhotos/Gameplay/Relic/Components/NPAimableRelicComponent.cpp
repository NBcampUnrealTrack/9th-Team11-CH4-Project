#include "Gameplay/Relic/Components/NPAimableRelicComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "CollisionQueryParams.h"
#include "Components/SceneComponent.h"
#include "Core/GameplayTag/NPGameplayTags.h"
#include "Engine/World.h"
#include "Gameplay/AbilitySystem/Effects/NPKnockbackGameplayEffect.h"
#include "Gameplay/Character/NPReplicatedStablePhysicsPawn.h"
#include "Gameplay/Relic/Abilities/NPRelicAimAbility.h"
#include "Gameplay/Relic/Abilities/NPRelicFireAbility.h"
#include "Gameplay/Relic/Projectile/NPAimableRelicVisualProjectile.h"
#include "GameplayEffect.h"
#include "GameFramework/Actor.h"
#include "NoPhotos.h"

UNPAimableRelicComponent::UNPAimableRelicComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	TArray<TSubclassOf<UGameplayAbility>> AbilityClasses;
	AbilityClasses.Add(UNPRelicAimAbility::StaticClass());
	AbilityClasses.Add(UNPRelicFireAbility::StaticClass());
	SetUseAbilityClasses(AbilityClasses);
	AimSettings.KnockbackEffectClass =
		UNPKnockbackGameplayEffect::StaticClass();
}

bool UNPAimableRelicComponent::TryFire(
	ANPReplicatedStablePhysicsPawn* ShooterPawn,
	UAbilitySystemComponent* SourceAbilitySystem,
	const FVector& CameraLocation,
	const FVector& CameraForward)
{
	AActor* Relic = GetOwner();
	UWorld* World = GetWorld();
	if (!IsValid(Relic)
		|| !Relic->HasAuthority()
		|| !IsValid(World)
		|| !IsValid(ShooterPawn)
		|| !IsValid(SourceAbilitySystem)
		|| !AimSettings.KnockbackEffectClass)
	{
		return false;
	}

	if (!TryConsumeFireCooldown())
	{
		return false;
	}

	const FVector TraceStart = CameraLocation;
	const FVector AimDirection = CameraForward.GetSafeNormal();
	if (AimDirection.IsNearlyZero())
	{
		return false;
	}
	FGameplayCueParameters FireCueParameters;
	FireCueParameters.Location = GetMuzzleTransform().GetLocation();
	FireCueParameters.Normal = AimDirection;
	FireCueParameters.Instigator = ShooterPawn;
	FireCueParameters.EffectCauser = Relic;
	SourceAbilitySystem->ExecuteGameplayCue(
		NPGameplayTags::GameplayCue_Relic_Aimable_Fire,
		FireCueParameters);

	const FVector TraceEnd = TraceStart
		+ AimDirection * FMath::Max(AimSettings.MaximumRange, 1.0f);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AimableRelicFire), true);
	QueryParams.AddIgnoredActor(ShooterPawn);
	QueryParams.AddIgnoredActor(Relic);
	FHitResult Hit;
	const bool bPrimaryHit = World->LineTraceSingleByChannel(
		Hit,
		TraceStart,
		TraceEnd,
		AimSettings.TraceChannel,
		QueryParams);

	if (bPrimaryHit)
	{
		UE_LOG(
			LogNoPhotos,
			Log,
			TEXT("[AimableRelic] Trace hit. Relic=%s Shooter=%s Actor=%s Component=%s Bone=%s Location=%s"),
			*GetNameSafe(Relic),
			*GetNameSafe(ShooterPawn),
			*GetNameSafe(Hit.GetActor()),
			*GetNameSafe(Hit.GetComponent()),
			*Hit.BoneName.ToString(),
			*Hit.ImpactPoint.ToCompactString());
	}
	else
	{
		UE_LOG(
			LogNoPhotos,
			Log,
			TEXT("[AimableRelic] Trace missed. Relic=%s Shooter=%s Start=%s End=%s"),
			*GetNameSafe(Relic),
			*GetNameSafe(ShooterPawn),
			*TraceStart.ToCompactString(),
			*TraceEnd.ToCompactString());
	}

	ANPStablePhysicsPawn* TargetPawn = bPrimaryHit
		? Cast<ANPStablePhysicsPawn>(Hit.GetActor())
		: nullptr;
	bool bAssistedHit = false;
	if (!IsValid(TargetPawn) && AimSettings.AimAssistRadius > 0.0f)
	{
		FHitResult AssistedHit;
		if (TryFindAssistedPlayer(
			World,
			TraceStart,
			TraceEnd,
			ShooterPawn,
			Relic,
			AssistedHit))
		{
			Hit = AssistedHit;
			TargetPawn = Cast<ANPStablePhysicsPawn>(Hit.GetActor());
			bAssistedHit = IsValid(TargetPawn);
			UE_LOG(
				LogNoPhotos,
				Log,
				TEXT("[AimableRelic] Aim assist selected player. Target=%s Component=%s Location=%s Radius=%.1f"),
				*GetNameSafe(TargetPawn),
				*GetNameSafe(Hit.GetComponent()),
				*Hit.ImpactPoint.ToCompactString(),
				AimSettings.AimAssistRadius);
		}
	}

	const FVector VisualStart = GetMuzzleTransform().GetLocation();
	const FVector VisualEnd = bPrimaryHit || bAssistedHit
		? FVector(Hit.ImpactPoint)
		: TraceEnd;
	UE_LOG(
		LogNoPhotos,
		Warning,
		TEXT("[VisualProjectile][Request] Relic=%s Class=%s Start=%s End=%s PrimaryHit=%s AssistedHit=%s"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(VisualProjectileClass),
		*VisualStart.ToCompactString(),
		*VisualEnd.ToCompactString(),
		bPrimaryHit ? TEXT("true") : TEXT("false"),
		bAssistedHit ? TEXT("true") : TEXT("false"));
	MulticastSpawnVisualProjectile(VisualStart, VisualEnd);
	if (bPrimaryHit || bAssistedHit)
	{
		FGameplayEffectContextHandle ImpactContext =
			SourceAbilitySystem->MakeEffectContext();
		ImpactContext.AddInstigator(ShooterPawn, Relic);
		ImpactContext.AddHitResult(Hit, true);
		FGameplayCueParameters ImpactCueParameters(ImpactContext);
		ImpactCueParameters.Location = Hit.ImpactPoint;
		ImpactCueParameters.Normal = Hit.ImpactNormal;
		SourceAbilitySystem->ExecuteGameplayCue(
			NPGameplayTags::GameplayCue_Relic_Aimable_Impact,
			ImpactCueParameters);
	}

	if (bPrimaryHit && !IsValid(TargetPawn))
	{
		UE_LOG(
			LogNoPhotos,
			Warning,
			TEXT("[AimableRelic] Pawn check failed. HitActor=%s Class=%s HitComponent=%s"),
			*GetNameSafe(Hit.GetActor()),
			Hit.GetActor()
				? *GetNameSafe(Hit.GetActor()->GetClass())
				: TEXT("None"),
			*GetNameSafe(Hit.GetComponent()));
		return true;
	}
	if (!IsValid(TargetPawn) || TargetPawn == ShooterPawn)
	{
		return true;
	}

	UAbilitySystemComponent* TargetAbilitySystem =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetPawn);
	if (!IsValid(TargetAbilitySystem))
	{
		UE_LOG(
			LogNoPhotos,
			Warning,
			TEXT("[AimableRelic] ASC lookup failed. TargetPawn=%s HitComponent=%s"),
			*GetNameSafe(TargetPawn),
			*GetNameSafe(Hit.GetComponent()));
		return true;
	}

	FVector HorizontalDirection(AimDirection.X, AimDirection.Y, 0.0f);
	HorizontalDirection.Normalize();
	if (HorizontalDirection.IsNearlyZero())
	{
		HorizontalDirection = ShooterPawn->GetActorForwardVector().GetSafeNormal2D();
	}
	const FVector KnockbackVelocity =
		HorizontalDirection * FMath::Max(AimSettings.HorizontalKnockbackStrength, 0.0f)
		+ FVector::UpVector * FMath::Max(AimSettings.UpwardKnockbackStrength, 0.0f);
	const float KnockbackMagnitude = KnockbackVelocity.Size();
	if (KnockbackMagnitude <= UE_SMALL_NUMBER)
	{
		return true;
	}

	FHitResult KnockbackHit = Hit;
	KnockbackHit.TraceStart = Hit.ImpactPoint;
	KnockbackHit.TraceEnd = Hit.ImpactPoint + KnockbackVelocity.GetSafeNormal();
	FGameplayEffectContextHandle EffectContext =
		SourceAbilitySystem->MakeEffectContext();
	EffectContext.AddSourceObject(Relic);
	EffectContext.AddHitResult(KnockbackHit, true);

	FGameplayEffectSpecHandle EffectSpec = SourceAbilitySystem->MakeOutgoingSpec(
		AimSettings.KnockbackEffectClass,
		1.0f,
		EffectContext);
	if (!EffectSpec.IsValid())
	{
		return true;
	}

	EffectSpec.Data->AddDynamicAssetTag(NPGameplayTags::Effect_Knockback);
	EffectSpec.Data->SetSetByCallerMagnitude(
		NPGameplayTags::Data_Knockback_Magnitude,
		KnockbackMagnitude);
	SourceAbilitySystem->ApplyGameplayEffectSpecToTarget(
		*EffectSpec.Data.Get(),
		TargetAbilitySystem);
	UE_LOG(
		LogNoPhotos,
		Log,
		TEXT("[AimableRelic] Knockback applied. Target=%s HitComponent=%s Direction=%s Magnitude=%.1f"),
		*GetNameSafe(TargetPawn),
		*GetNameSafe(Hit.GetComponent()),
		*KnockbackVelocity.GetSafeNormal().ToCompactString(),
		KnockbackMagnitude);
	return true;
}

bool UNPAimableRelicComponent::TryConsumeFireCooldown()
{
	const UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return false;
	}

	const double CurrentTime = World->GetTimeSeconds();
	if (CurrentTime - LastServerFireTime
		< FMath::Max(AimSettings.FireInterval, 0.0f))
	{
		return false;
	}

	LastServerFireTime = CurrentTime;
	return true;
}

FTransform UNPAimableRelicComponent::GetMuzzleTransform() const
{
	const AActor* Relic = GetOwner();
	const USceneComponent* RelicMesh = Relic
		? Cast<USceneComponent>(Relic->GetRootComponent())
		: nullptr;
	return IsValid(RelicMesh)
		? RelicMesh->GetSocketTransform(MuzzleSocketName)
		: FTransform::Identity;
}

bool UNPAimableRelicComponent::TryFindAssistedPlayer(
	UWorld* World,
	const FVector& TraceStart,
	const FVector& TraceEnd,
	ANPReplicatedStablePhysicsPawn* ShooterPawn,
	AActor* Relic,
	FHitResult& OutHit) const
{
	if (!IsValid(World)
		|| !IsValid(ShooterPawn)
		|| AimSettings.AimAssistRadius <= 0.0f)
	{
		return false;
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);

	FCollisionQueryParams SweepQueryParams(
		SCENE_QUERY_STAT(AimableRelicAssist),
		false);
	SweepQueryParams.AddIgnoredActor(ShooterPawn);
	SweepQueryParams.AddIgnoredActor(Relic);

	TArray<FHitResult> SweepHits;
	World->SweepMultiByObjectType(
		SweepHits,
		TraceStart,
		TraceEnd,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(AimSettings.AimAssistRadius),
		SweepQueryParams);

	const FVector TraceDirection = (TraceEnd - TraceStart).GetSafeNormal();
	const float TraceLength = FVector::Distance(TraceStart, TraceEnd);
	float BestScore = TNumericLimits<float>::Max();
	TSet<TWeakObjectPtr<ANPStablePhysicsPawn>> EvaluatedPawns;
	for (const FHitResult& SweepHit : SweepHits)
	{
		ANPStablePhysicsPawn* CandidatePawn = Cast<ANPStablePhysicsPawn>(
			SweepHit.GetActor());
		const TWeakObjectPtr<ANPStablePhysicsPawn> CandidateKey(
			CandidatePawn);
		if (!IsValid(CandidatePawn)
			|| CandidatePawn == ShooterPawn
			|| EvaluatedPawns.Contains(CandidateKey))
		{
			continue;
		}
		EvaluatedPawns.Add(CandidateKey);

		FVector TargetOrigin;
		FVector TargetExtent;
		CandidatePawn->GetActorBounds(true, TargetOrigin, TargetExtent);
		const float ForwardDistance = FVector::DotProduct(
			TargetOrigin - TraceStart,
			TraceDirection);
		if (ForwardDistance <= 0.0f || ForwardDistance > TraceLength)
		{
			continue;
		}

		const FVector ClosestRayPoint = TraceStart
			+ TraceDirection * ForwardDistance;
		const float LateralDistanceSquared = FVector::DistSquared(
			TargetOrigin,
			ClosestRayPoint);

		FHitResult VisibilityHit;
		FCollisionQueryParams VisibilityQueryParams(
			SCENE_QUERY_STAT(AimableRelicAssistVisibility),
			true);
		VisibilityQueryParams.AddIgnoredActor(ShooterPawn);
		VisibilityQueryParams.AddIgnoredActor(Relic);
		const bool bVisibilityBlocked = World->LineTraceSingleByChannel(
			VisibilityHit,
			TraceStart,
			TargetOrigin,
			AimSettings.TraceChannel,
			VisibilityQueryParams);
		if (bVisibilityBlocked
			&& VisibilityHit.GetActor() != CandidatePawn)
		{
			UE_LOG(
				LogNoPhotos,
				Verbose,
				TEXT("[AimableRelic] Aim assist candidate occluded. Candidate=%s Blocker=%s Component=%s"),
				*GetNameSafe(CandidatePawn),
				*GetNameSafe(VisibilityHit.GetActor()),
				*GetNameSafe(VisibilityHit.GetComponent()));
			continue;
		}

		const float Score = LateralDistanceSquared
			+ ForwardDistance * 0.01f;
		if (Score >= BestScore)
		{
			continue;
		}

		BestScore = Score;
		OutHit = bVisibilityBlocked ? VisibilityHit : SweepHit;
	}

	return BestScore < TNumericLimits<float>::Max();
}

void UNPAimableRelicComponent::MulticastSpawnVisualProjectile_Implementation(
	const FVector_NetQuantize10 StartLocation,
	const FVector_NetQuantize10 EndLocation)
{
	const TCHAR* NetModeText = TEXT("Unknown");
	switch (GetNetMode())
	{
	case NM_Standalone:
		NetModeText = TEXT("Standalone");
		break;
	case NM_DedicatedServer:
		NetModeText = TEXT("DedicatedServer");
		break;
	case NM_ListenServer:
		NetModeText = TEXT("ListenServer");
		break;
	case NM_Client:
		NetModeText = TEXT("Client");
		break;
	default:
		break;
	}

	UE_LOG(
		LogNoPhotos,
		Warning,
		TEXT("[VisualProjectile][Multicast] Owner=%s NetMode=%s Class=%s Start=%s End=%s"),
		*GetNameSafe(GetOwner()),
		NetModeText,
		*GetNameSafe(VisualProjectileClass),
		*FVector(StartLocation).ToCompactString(),
		*FVector(EndLocation).ToCompactString());

	if (!VisualProjectileClass)
	{
		UE_LOG(
			LogNoPhotos,
			Error,
			TEXT("[VisualProjectile][Multicast] Rejected: VisualProjectileClass is not assigned. Owner=%s"),
			*GetNameSafe(GetOwner()));
		return;
	}

	UWorld* World = GetWorld();
	const FVector Direction = (FVector(EndLocation) - FVector(StartLocation))
		.GetSafeNormal();
	if (!IsValid(World) || Direction.IsNearlyZero())
	{
		UE_LOG(
			LogNoPhotos,
			Error,
			TEXT("[VisualProjectile][Multicast] Rejected: invalid world or direction. Owner=%s World=%s Direction=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(World),
			*Direction.ToCompactString());
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = GetOwner();
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ANPAimableRelicVisualProjectile* VisualProjectile =
		World->SpawnActor<ANPAimableRelicVisualProjectile>(
			VisualProjectileClass,
			FVector(StartLocation),
			Direction.Rotation(),
			SpawnParameters);
	UE_LOG(
		LogNoPhotos,
		Warning,
		TEXT("[VisualProjectile][Spawn] Result=%s Actor=%s Class=%s Location=%s"),
		IsValid(VisualProjectile) ? TEXT("success") : TEXT("failed"),
		*GetNameSafe(VisualProjectile),
		*GetNameSafe(VisualProjectileClass),
		*FVector(StartLocation).ToCompactString());
	if (IsValid(VisualProjectile))
	{
		VisualProjectile->InitializeVisualProjectile(
			FVector(StartLocation),
			FVector(EndLocation));
	}
}
