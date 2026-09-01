#include "Gameplay/MapEvents/Bomb/NPTimedBomb.h"

#include "AbilitySystemComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/GameplayTag/NPGameplayTags.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Gameplay/AbilitySystem/Effects/NPKnockbackGameplayEffect.h"
#include "Gameplay/AbilitySystem/NPAbilitySystemComponent.h"
#include "Gameplay/Character/NPStablePhysicsPawn.h"
#include "Gameplay/Interaction/Components/GrabbableComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogNPTimedBomb, Log, All);

ANPTimedBomb::ANPTimedBomb(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsDisplayed = false;
	bAlwaysRelevant = true;
	ExplosionAbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("ExplosionAbilitySystem"));
	// 서버의 효과 발신용 ASC입니다. 넉백 동기화는 기존 대상 캐릭터 경로를 사용합니다.
	ExplosionAbilitySystem->SetIsReplicated(false);
	KnockbackEffectClass = UNPKnockbackGameplayEffect::StaticClass();

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaceholderMesh(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(RelicMesh))
	{
		if (PlaceholderMesh.Succeeded())
		{
			Mesh->SetStaticMesh(PlaceholderMesh.Object);
			Mesh->SetRelativeScale3D(FVector(0.4));
		}
	}
}

UAbilitySystemComponent* ANPTimedBomb::GetAbilitySystemComponent() const
{
	return ExplosionAbilitySystem;
}

void ANPTimedBomb::BeginPlay()
{
	Super::BeginPlay();
	if (bHasExploded)
	{
		ApplyExplodedState();
		return;
	}
	if (HasAuthority())
	{
		ExplosionAbilitySystem->InitAbilityActorInfo(this, this);
		const float Delay = FMath::IsFinite(FuseDuration) ? FMath::Max(0.1f, FuseDuration) : 5.0f;
		const AGameStateBase* GameState = GetWorld()->GetGameState();
		const float ServerTime = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
		ExplosionServerTime = ServerTime + Delay;
		GetWorldTimerManager().SetTimer(FuseTimer, this, &ThisClass::Explode, Delay, false);
		ForceNetUpdate();
	}
}

void ANPTimedBomb::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANPTimedBomb, bHasExploded);
	DOREPLIFETIME(ANPTimedBomb, ExplosionServerTime);
}

float ANPTimedBomb::GetRemainingFuseTime() const
{
	const UWorld* World = GetWorld();
	if (bHasExploded || !World || ExplosionServerTime <= 0.0f)
	{
		return 0.0f;
	}
	const AGameStateBase* GameState = World->GetGameState();
	if (!HasAuthority() && !GameState)
	{
		return 0.0f;
	}
	const float ServerTime = GameState ? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
	return FMath::Max(0.0f, ExplosionServerTime - ServerTime);
}

FVector ANPTimedBomb::CalculateBlastVelocity(const FVector& Origin, const FVector& Target,
	const float Radius, const float HorizontalStrength, const float UpwardStrength)
{
	if (Origin.ContainsNaN() || Target.ContainsNaN() || !FMath::IsFinite(Radius) || Radius <= 0.0f ||
		!FMath::IsFinite(HorizontalStrength) || !FMath::IsFinite(UpwardStrength) ||
		FVector::DistSquared(Origin, Target) > FMath::Square(static_cast<double>(Radius)))
	{
		return FVector::ZeroVector;
	}
	return (Target - Origin).GetSafeNormal2D() * FMath::Max(0.0f, HorizontalStrength)
		+ FVector::UpVector * FMath::Max(0.0f, UpwardStrength);
}

void ANPTimedBomb::Explode()
{
	if (!HasAuthority() || bHasExploded || IsActorBeingDestroyed())
	{
		return;
	}
	const FVector Origin = GetActorLocation();
	FlushNetDormancy();
	bHasExploded = true;
	GetWorldTimerManager().ClearTimer(FuseTimer);
	ApplyExplodedState();
	ApplyBlastKnockback(Origin);
	MulticastExplosion(Origin);
	if (!IsActorBeingDestroyed())
	{
		ForceNetUpdate();
		// 파괴 복제 전에 폭발 상태/RPC가 전달될 여유를 둡니다. 외형과 충돌은 이미 제거됩니다.
		SetLifeSpan(2.0f);
	}
}

void ANPTimedBomb::ApplyBlastKnockback(const FVector& Origin)
{
	if (!HasAuthority() || !GetWorld() || !KnockbackEffectClass)
	{
		return;
	}
	int32 AffectedCount = 0;
	// 여러 신체/충돌 컴포넌트를 가진 캐릭터도 액터당 한 번만 적용합니다.
	for (TActorIterator<ANPStablePhysicsPawn> It(GetWorld()); It; ++It)
	{
		ANPStablePhysicsPawn* Target = *It;
		if (!IsValid(Target) || Target->IsActorBeingDestroyed())
		{
			continue;
		}
		const FVector TargetLocation = Target->GetActorLocation();
		const FVector Velocity = CalculateBlastVelocity(Origin, TargetLocation,
			ExplosionRadius, HorizontalKnockbackStrength, UpwardKnockbackStrength);
		if (Velocity.IsNearlyZero())
		{
			continue;
		}
		UNPAbilitySystemComponent* TargetASC = Target->FindComponentByClass<UNPAbilitySystemComponent>();
		if (!IsValid(TargetASC) || TargetASC->GetAvatarActor() != Target)
		{
			continue;
		}
		if (bRequireLineOfSight)
		{
			FCollisionQueryParams Query(SCENE_QUERY_STAT(TimedBombVisibility), false, this);
			Query.AddIgnoredActor(Target);
			FHitResult Obstruction;
			if (GetWorld()->LineTraceSingleByChannel(Obstruction, Origin, TargetLocation, ECC_Visibility, Query))
			{
				continue;
			}
		}
		FHitResult KnockbackHit;
		KnockbackHit.ImpactPoint = TargetLocation;
		KnockbackHit.TraceStart = Origin;
		KnockbackHit.TraceEnd = Origin + Velocity.GetSafeNormal();
		FGameplayEffectContextHandle Context = ExplosionAbilitySystem->MakeEffectContext();
		Context.AddSourceObject(this);
		Context.AddHitResult(KnockbackHit, true);
		FGameplayEffectSpecHandle Spec = ExplosionAbilitySystem->MakeOutgoingSpec(KnockbackEffectClass, 1.0f, Context);
		if (!Spec.IsValid())
		{
			continue;
		}
		Spec.Data->AddDynamicAssetTag(NPGameplayTags::Effect_Knockback);
		Spec.Data->SetSetByCallerMagnitude(NPGameplayTags::Data_Knockback_Magnitude, Velocity.Size());
		ExplosionAbilitySystem->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
		++AffectedCount;
	}
	UE_LOG(LogNPTimedBomb, Log, TEXT("폭탄 폭발: Bomb=%s Location=%s Radius=%.1f GAS 적용 시도=%d"),
		*GetName(), *Origin.ToCompactString(), ExplosionRadius, AffectedCount);
}

void ANPTimedBomb::ApplyExplodedState()
{
	if (!bHasExploded)
	{
		return;
	}
	GrabbableComponent->SetGrabEnabled(false);
	RelicMesh->SetSimulatePhysics(false);
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);
	SetReplicateMovement(false);
}

void ANPTimedBomb::OnRep_HasExploded()
{
	ApplyExplodedState();
}

void ANPTimedBomb::MulticastExplosion_Implementation(const FVector_NetQuantize ExplosionLocation)
{
	bHasExploded = true;
	ApplyExplodedState();
	if (GetNetMode() == NM_DedicatedServer || bExplosionPresentationPlayed)
	{
		return;
	}
	bExplosionPresentationPlayed = true;
	if (ExplosionSystem)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ExplosionSystem, ExplosionLocation);
	}
	if (ExplosionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, ExplosionLocation);
	}
#if ENABLE_DRAW_DEBUG
	if (bDrawDebugExplosion && FMath::IsFinite(ExplosionRadius) && ExplosionRadius > 0.0f)
	{
		DrawDebugSphere(GetWorld(), ExplosionLocation, ExplosionRadius, 24, FColor::Orange, false, 0.75f, 0, 3.0f);
	}
#endif
	OnBombExploded(ExplosionLocation);
}

void ANPTimedBomb::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(FuseTimer);
	GrabbableComponent->SetGrabEnabled(false);
	Super::EndPlay(EndPlayReason);
}
