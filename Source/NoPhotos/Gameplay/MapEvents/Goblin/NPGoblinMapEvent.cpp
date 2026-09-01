#include "NPGoblinMapEvent.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "Gameplay/Goblin/NPGoblinCharacter.h"
#include "Gameplay/Goblin/NPGoblinPatrolRoute.h"
#include "Kismet/GameplayStatics.h"
#include "Gameplay/MapEvents/NPMapEventManager.h"
#include "Gameplay/MapEvents/NPMapEventSpawnVolume.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogNPGoblinMapEvent, Log, All);

ANPGoblinMapEvent::ANPGoblinMapEvent()
{
	EventId = TEXT("Goblin");
	EventDisplayName = NSLOCTEXT("MapEvent", "GoblinEventName", "고블린");
	EventType = ENPMapEventType::TypeA;
	EventScale = ENPMapEventScale::Medium;
	Duration = 60.0f;
	LocationSource = ENPMapEventLocationSource::Volume;
	GoblinSpawnGroup = FGameplayTag::RequestGameplayTag(FName(TEXT("Goblin")), false);
}

void ANPGoblinMapEvent::ApplyEventState_Implementation(const bool bNewActive)
{
	if (!HasAuthority())
	{
		return;
	}

	if (bNewActive)
	{
		bAllowRespawning = true;
		TrySpawnGoblin();
		return;
	}

	bAllowRespawning = false;
	GetWorldTimerManager().ClearTimer(SpawnTimer);
	BeginDespawnSpawnedGoblins();
}

void ANPGoblinMapEvent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bAllowRespawning = false;
	GetWorldTimerManager().ClearTimer(SpawnTimer);
	if (HasAuthority())
	{
		DestroySpawnedGoblinsImmediately();
	}

	Super::EndPlay(EndPlayReason);
}

bool ANPGoblinMapEvent::CanSpawnGoblin() const
{
	return HasAuthority() && bAllowRespawning && IsEventActive()
		&& !IsActorBeingDestroyed()
		&& (GetEventDuration() <= 0.0f || GetRemainingEventTime() > 0.0f);
}

void ANPGoblinMapEvent::ScheduleSpawn(const float Delay)
{
	if (CanSpawnGoblin() && SpawnedGoblins.IsEmpty())
	{
		GetWorldTimerManager().SetTimer(SpawnTimer, this, &ThisClass::TrySpawnGoblin,
			FMath::IsFinite(Delay) ? FMath::Max(0.1f, Delay) : 2.0f, false);
	}
}

void ANPGoblinMapEvent::HandleGoblinDestroyed(AActor* DestroyedActor)
{
	const int32 Removed = SpawnedGoblins.RemoveAll(
		[DestroyedActor](const TObjectPtr<ANPGoblinCharacter>& Goblin)
		{
			return Goblin.Get() == DestroyedActor;
		});
	if (Removed > 0)
	{
		// HP 0 starts the existing door exit. Only destruction after that exit frees the slot.
		UE_LOG(LogNPGoblinMapEvent, Log, TEXT("고블린 퇴장 완료: Actor=%s Remaining=%.2fs"),
			*GetNameSafe(DestroyedActor), GetRemainingEventTime());
		ScheduleSpawn(RespawnDelay);
	}
}

void ANPGoblinMapEvent::TrySpawnGoblin()
{
	GetWorldTimerManager().ClearTimer(SpawnTimer);
	if (!CanSpawnGoblin() || !SpawnedGoblins.IsEmpty())
	{
		return;
	}

	UNPMapEventManagerComponent* EventManager = GetOwner()
		? GetOwner()->FindComponentByClass<UNPMapEventManagerComponent>()
		: nullptr;
	if (!EventManager || !GoblinClass || !GoblinSpawnGroup.IsValid())
	{
		UE_LOG(
			LogNPGoblinMapEvent,
			Error,
			TEXT("고블린 생성 준비 실패: Manager=%s, GoblinClass=%s, SpawnGroup=%s"),
			*GetNameSafe(EventManager),
			*GetNameSafe(GoblinClass),
			*GoblinSpawnGroup.ToString());
		return;
	}

	const int32 AttemptsPerGoblin = FMath::Max(1, MaximumSpawnAttemptsPerGoblin);
	FVector RequiredHalfExtent(
		FMath::Max(1.0f, GoblinRequiredHalfExtent.X),
		FMath::Max(1.0f, GoblinRequiredHalfExtent.Y),
		FMath::Max(1.0f, GoblinRequiredHalfExtent.Z));
	const ANPGoblinCharacter* Defaults = GoblinClass.GetDefaultObject();
	if (const UCapsuleComponent* Capsule = Defaults ? Defaults->GetCapsuleComponent() : nullptr)
	{
		RequiredHalfExtent.X = FMath::Max(RequiredHalfExtent.X, Capsule->GetScaledCapsuleRadius());
		RequiredHalfExtent.Y = FMath::Max(RequiredHalfExtent.Y, Capsule->GetScaledCapsuleRadius());
		RequiredHalfExtent.Z = FMath::Max(RequiredHalfExtent.Z, Capsule->GetScaledCapsuleHalfHeight());
	}
	ANPGoblinPatrolRoute* PatrolRoute = FindPatrolRoute();
	if (!PatrolRoute)
	{
		UE_LOG(
			LogNPGoblinMapEvent,
			Warning,
			TEXT("고블린 생성 대기: 유효한 폐곡선 루트가 없습니다. SpawnGroup=%s (로드된 레벨 인스턴스의 RouteGroup/ClosedLoop 확인)"),
			*GoblinSpawnGroup.ToString());
		ScheduleSpawn(SpawnRetryInterval);
		return;
	}

	for (int32 Attempt = 0; Attempt < AttemptsPerGoblin; ++Attempt)
	{
		FTransform GroundTransform;
		// Existing weighted volume selection is repeated for every new goblin; routes never supply spawns.
		if (!EventManager->FindRandomSpawnTransformBySource(
			GoblinSpawnGroup, RequiredHalfExtent, ENPMapEventLocationSource::Volume, GroundTransform))
		{
			continue;
		}

		if (ANPGoblinCharacter* Goblin = SpawnGoblinAt(GroundTransform, PatrolRoute))
		{
			UE_LOG(LogNPGoblinMapEvent, Display,
				TEXT("고블린 생성 성공: Actor=%s Route=%s Location=%s Remaining=%.2fs (동시 1마리)"),
				*GetNameSafe(Goblin), *GetNameSafe(PatrolRoute),
				*Goblin->GetActorLocation().ToCompactString(), GetRemainingEventTime());
			return;
		}
		if (!CanSpawnGoblin())
		{
			return;
		}
	}

	TArray<ANPMapEventSpawnVolume*> Volumes;
	EventManager->GetSpawnVolumesForGroup(GoblinSpawnGroup, Volumes);
	UE_LOG(LogNPGoblinMapEvent, Warning,
		TEXT("고블린 생성 실패, 재시도 예정: Attempts=%d Volumes=%d Group=%s"),
		AttemptsPerGoblin, Volumes.Num(), *GoblinSpawnGroup.ToString());
	for (const ANPMapEventSpawnVolume* Volume : Volumes)
	{
		if (IsValid(Volume))
		{
			UE_LOG(LogNPGoblinMapEvent, Warning, TEXT("  Volume=%s Weight=%.2f Failure=%s"),
				*GetNameSafe(Volume), Volume->GetSelectionWeight(), *Volume->GetLastSpawnFailureReason());
		}
	}
	ScheduleSpawn(SpawnRetryInterval);
}

ANPGoblinPatrolRoute* ANPGoblinMapEvent::FindPatrolRoute() const
{
	UWorld* World = GetWorld();
	if (!World || !GoblinSpawnGroup.IsValid())
	{
		return nullptr;
	}

	TArray<ANPGoblinPatrolRoute*> Candidates;
	for (TActorIterator<ANPGoblinPatrolRoute> Iterator(World); Iterator; ++Iterator)
	{
		ANPGoblinPatrolRoute* Route = *Iterator;
		if (IsValid(Route)
			&& Route->SupportsRouteGroup(GoblinSpawnGroup)
			&& Route->IsUsableRoute())
		{
			Candidates.Add(Route);
		}
	}

	return Candidates.IsEmpty()
		? nullptr
		: Candidates[FMath::RandHelper(Candidates.Num())];
}

ANPGoblinCharacter* ANPGoblinMapEvent::SpawnGoblinAt(
	const FTransform& GroundTransform,
	ANPGoblinPatrolRoute* PatrolRoute)
{
	UWorld* World = GetWorld();
	if (!World || !GoblinClass)
	{
		return nullptr;
	}

	FTransform SpawnTransform = GroundTransform;
	const ANPGoblinCharacter* Defaults = GoblinClass.GetDefaultObject();
	const UCapsuleComponent* Capsule = Defaults ? Defaults->GetCapsuleComponent() : nullptr;
	const float MinimumHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() + 2.0f : 2.0f;
	SpawnTransform.AddToTranslation(
		FVector::UpVector * FMath::Max(MinimumHeight, GoblinSpawnHeightOffset));

	ANPGoblinCharacter* Goblin = World->SpawnActorDeferred<ANPGoblinCharacter>(
		GoblinClass,
		SpawnTransform,
		this,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Goblin)
	{
		return nullptr;
	}

	Goblin->SetReplicates(true);
	Goblin->SetReplicateMovement(true);
	Goblin->SetPatrolRoute(PatrolRoute);
	Goblin->PrepareForSpawnPresentation();
	// Register before BeginPlay: a BP can finish the event or destroy itself during initialization.
	SpawnedGoblins.Add(Goblin);
	Goblin->OnDestroyed.AddDynamic(this, &ThisClass::HandleGoblinDestroyed);
	UGameplayStatics::FinishSpawningActor(Goblin, SpawnTransform);
	return IsValid(Goblin) && !Goblin->IsActorBeingDestroyed() ? Goblin : nullptr;
}

void ANPGoblinMapEvent::BeginDespawnSpawnedGoblins()
{
	// BeginDespawnPresentation can destroy immediately if there is no room for the door.
	const TArray<TObjectPtr<ANPGoblinCharacter>> GoblinsToDespawn = SpawnedGoblins;
	for (ANPGoblinCharacter* Goblin : GoblinsToDespawn)
	{
		if (IsValid(Goblin))
		{
			Goblin->BeginDespawnPresentation();
		}
	}
}

void ANPGoblinMapEvent::DestroySpawnedGoblinsImmediately()
{
	for (ANPGoblinCharacter* Goblin : SpawnedGoblins)
	{
		if (IsValid(Goblin))
		{
			Goblin->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleGoblinDestroyed);
			Goblin->Destroy();
		}
	}

	SpawnedGoblins.Reset();
}
