#include "Gameplay/Relic/Components/NPFireballRelicComponent.h"

#include "AbilitySystemComponent.h"
#include "Core/GameplayTag/NPGameplayTags.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Gameplay/AbilitySystem/Effects/NPKnockbackGameplayEffect.h"
#include "Gameplay/Character/NPReplicatedStablePhysicsPawn.h"
#include "Gameplay/Interaction/Components/GrabbableComponent.h"
#include "Gameplay/Relic/NPBaseRelic.h"
#include "GameplayEffect.h"
#include "TimerManager.h"

UNPFireballRelicComponent::UNPFireballRelicComponent()
{
	ProjectileClass = ANPFireballProjectile::StaticClass();
	ExplosionSettings.KnockbackEffectClass =
		UNPKnockbackGameplayEffect::StaticClass();
}

bool UNPFireballRelicComponent::TryFire(
	ANPReplicatedStablePhysicsPawn* ShooterPawn,
	UAbilitySystemComponent* SourceAbilitySystem,
	const FVector& CameraLocation,
	const FVector& CameraForward)
{
	ANPBaseRelic* Relic = Cast<ANPBaseRelic>(GetOwner());
	UWorld* World = GetWorld();
	const FVector AimDirection = CameraForward.GetSafeNormal();
	if (!IsValid(Relic)
		|| !Relic->HasAuthority()
		|| !IsValid(World)
		|| !IsValid(ShooterPawn)
		|| !IsValid(SourceAbilitySystem)
		|| !ProjectileClass
		|| AimDirection.IsNearlyZero()
		|| !TryConsumeFireCooldown())
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
	const float Delay = FMath::Max(FireDelay, 0.0f);
	if (Delay <= UE_SMALL_NUMBER)
	{
		ExecuteFire(
			ShooterPawn,
			SourceAbilitySystem,
			CameraLocation,
			CameraForward);
		return true;
	}

	FTimerDelegate FireDelegate;
	FireDelegate.BindUObject(
		this,
		&UNPFireballRelicComponent::ExecuteFire,
		TWeakObjectPtr<ANPReplicatedStablePhysicsPawn>(ShooterPawn),
		TWeakObjectPtr<UAbilitySystemComponent>(SourceAbilitySystem),
		CameraLocation,
		CameraForward);
	FTimerHandle FireTimer;
	World->GetTimerManager().SetTimer(
		FireTimer,
		FireDelegate,
		Delay,
		false);
	return true;
}

void UNPFireballRelicComponent::ExecuteFire(
	TWeakObjectPtr<ANPReplicatedStablePhysicsPawn> ShooterPawn,
	TWeakObjectPtr<UAbilitySystemComponent> SourceAbilitySystem,
	const FVector CameraLocation,
	const FVector CameraForward)
{
	ANPBaseRelic* Relic = Cast<ANPBaseRelic>(GetOwner());
	UWorld* World = GetWorld();
	ANPReplicatedStablePhysicsPawn* Shooter = ShooterPawn.Get();
	UAbilitySystemComponent* SourceASC = SourceAbilitySystem.Get();
	FRotator AimRotation = CameraForward.Rotation();
	AimRotation.Pitch = FRotator::NormalizeAxis(
		AimRotation.Pitch + LaunchPitchOffset);
	const FVector AimDirection = AimRotation.Vector().GetSafeNormal();
	if (!IsValid(Relic)
		|| !Relic->HasAuthority()
		|| !IsValid(World)
		|| !IsValid(Shooter)
		|| !IsValid(SourceASC)
		|| !ProjectileClass
		|| AimDirection.IsNearlyZero())
	{
		return;
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			INDEX_NONE,
			5.0f,
			FColor::Cyan,
			TEXT("[Fireball 1/4] 최종 투사체 발사 로직 실행"));
	}

	const FVector AimPoint = CameraLocation
		+ AimDirection * FMath::Max(GetAimSettings().MaximumRange, 1.0f);
	const FVector SpawnOrigin = Shooter->GetActorLocation()
		+ FVector::UpVector * SpawnUpwardOffset;
	FVector LaunchDirection = (AimPoint - SpawnOrigin)
		.GetSafeNormal();
	if (LaunchDirection.IsNearlyZero())
	{
		LaunchDirection = AimDirection;
	}

	const FVector SpawnLocation = SpawnOrigin
		+ LaunchDirection * FMath::Max(SpawnForwardOffset, 0.0f);
	const FTransform SpawnTransform(
		LaunchDirection.Rotation(),
		SpawnLocation);
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = Relic;
	SpawnParameters.Instigator = Shooter;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ANPFireballProjectile* Projectile = World->SpawnActor<ANPFireballProjectile>(
		ProjectileClass,
		SpawnTransform,
		SpawnParameters);
	if (!IsValid(Projectile))
	{
		return;
	}

	Projectile->InitializeProjectile(
		SourceASC,
		Relic,
		LaunchDirection * FMath::Max(LaunchSpeed, 1.0f),
		ExplosionSettings,
		ProjectileLifeTime);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			INDEX_NONE,
			5.0f,
			FColor::Green,
			FString::Printf(
				TEXT("[Fireball 2/4] 투사체 생성: %s"),
				*GetNameSafe(Projectile)));
	}
	if (UGrabbableComponent* Grabbable =
		Relic->FindComponentByClass<UGrabbableComponent>())
	{
		Grabbable->ForceReleaseAllGrabs();
	}
	const FVector RecoilVelocity =
		-LaunchDirection * FMath::Max(RecoilBackwardSpeed, 0.0f)
		+ FVector::UpVector * FMath::Max(RecoilUpwardSpeed, 0.0f);
	Relic->ReleaseWithVelocityImpulse(RecoilVelocity);
	ApplyShooterRecoil(
		Shooter,
		SourceASC,
		LaunchDirection);
}

void UNPFireballRelicComponent::ApplyShooterRecoil(
	ANPReplicatedStablePhysicsPawn* ShooterPawn,
	UAbilitySystemComponent* SourceAbilitySystem,
	const FVector& LaunchDirection) const
{
	if (!IsValid(ShooterPawn) || !IsValid(SourceAbilitySystem))
	{
		return;
	}

	const FVector RecoilVelocity =
		-LaunchDirection * FMath::Max(ShooterRecoilBackwardSpeed, 0.0f)
		+ FVector::UpVector * FMath::Max(ShooterRecoilUpwardSpeed, 0.0f);
	if (!RecoilVelocity.IsNearlyZero()
		&& ExplosionSettings.KnockbackEffectClass)
	{
		FHitResult RecoilHit;
		RecoilHit.TraceStart = ShooterPawn->GetActorLocation();
		RecoilHit.TraceEnd = RecoilHit.TraceStart
			+ RecoilVelocity.GetSafeNormal();
		FGameplayEffectContextHandle EffectContext =
			SourceAbilitySystem->MakeEffectContext();
		EffectContext.AddSourceObject(GetOwner());
		EffectContext.AddHitResult(RecoilHit, true);

		FGameplayEffectSpecHandle EffectSpec =
			SourceAbilitySystem->MakeOutgoingSpec(
				ExplosionSettings.KnockbackEffectClass,
				1.0f,
				EffectContext);
		if (EffectSpec.IsValid())
		{
			EffectSpec.Data->AddDynamicAssetTag(
				NPGameplayTags::Effect_Knockback);
			EffectSpec.Data->SetSetByCallerMagnitude(
				NPGameplayTags::Data_Knockback_Magnitude,
				RecoilVelocity.Size());
			SourceAbilitySystem->ApplyGameplayEffectSpecToSelf(
				*EffectSpec.Data.Get());
		}
	}
}
