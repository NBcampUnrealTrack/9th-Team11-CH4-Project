#include "NPHotPotatoBomb.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

ANPHotPotatoBomb::ANPHotPotatoBomb()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(true);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	BombMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BombMesh"));
	BombMesh->SetupAttachment(SceneRoot);
	BombMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BombMesh->SetGenerateOverlapEvents(false);
	BombMesh->SetSimulatePhysics(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaceholderMesh(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (PlaceholderMesh.Succeeded())
	{
		BombMesh->SetStaticMesh(PlaceholderMesh.Object);
		BombMesh->SetRelativeScale3D(FVector(0.3f));
	}
}

void ANPHotPotatoBomb::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANPHotPotatoBomb, bHasExploded);
	DOREPLIFETIME(ANPHotPotatoBomb, ExplosionServerWorldTime);
}

void ANPHotPotatoBomb::InitializeFuse(const float InExplosionServerWorldTime)
{
	if (!HasAuthority() || bHasExploded)
	{
		return;
	}
	ExplosionServerWorldTime = FMath::Max(0.0f, InExplosionServerWorldTime);
	BP_OnBombArmed(ExplosionServerWorldTime);
	ForceNetUpdate();
}

void ANPHotPotatoBomb::TriggerExplosion()
{
	if (!HasAuthority() || bHasExploded || IsActorBeingDestroyed())
	{
		return;
	}

	const FVector ExplosionLocation = GetActorLocation();
	bHasExploded = true;
	ExplosionServerWorldTime = 0.0f;
	ApplyExplodedState();
	MulticastExplosion(ExplosionLocation);
	ForceNetUpdate();
	SetLifeSpan(FMath::Max(0.1f, DestroyDelayAfterExplosion));
}

float ANPHotPotatoBomb::GetRemainingFuseTime() const
{
	const UWorld* World = GetWorld();
	if (!World || bHasExploded || ExplosionServerWorldTime <= 0.0f)
	{
		return 0.0f;
	}
	const AGameStateBase* GameState = World->GetGameState();
	const float ServerWorldTime = GameState
		? GameState->GetServerWorldTimeSeconds()
		: World->GetTimeSeconds();
	return FMath::Max(0.0f, ExplosionServerWorldTime - ServerWorldTime);
}

void ANPHotPotatoBomb::ApplyExplodedState()
{
	if (!bHasExploded)
	{
		return;
	}
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);
	SetReplicateMovement(false);
}

void ANPHotPotatoBomb::PlayExplosionPresentation(
	const FVector& ExplosionLocation)
{
	if (GetNetMode() == NM_DedicatedServer || bExplosionPresentationPlayed)
	{
		return;
	}
	bExplosionPresentationPlayed = true;
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
	OnBombExploded.Broadcast(ExplosionLocation);
}

void ANPHotPotatoBomb::OnRep_HasExploded()
{
	ApplyExplodedState();
	if (bHasExploded)
	{
		PlayExplosionPresentation(GetActorLocation());
	}
}

void ANPHotPotatoBomb::OnRep_ExplosionServerWorldTime()
{
	if (!bHasExploded && ExplosionServerWorldTime > 0.0f)
	{
		BP_OnBombArmed(ExplosionServerWorldTime);
	}
}

void ANPHotPotatoBomb::MulticastExplosion_Implementation(
	const FVector_NetQuantize ExplosionLocation)
{
	bHasExploded = true;
	ExplosionServerWorldTime = 0.0f;
	ApplyExplodedState();
	PlayExplosionPresentation(ExplosionLocation);
}
