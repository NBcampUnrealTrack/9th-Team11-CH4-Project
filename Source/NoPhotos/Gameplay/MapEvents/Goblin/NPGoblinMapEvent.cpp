#include "NPGoblinMapEvent.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Gameplay/Goblin/NPGoblinCharacter.h"
#include "Gameplay/Goblin/NPGoblinPatrolRoute.h"
#include "Kismet/GameplayStatics.h"
#include "Gameplay/MapEvents/NPMapEventManager.h"

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
		SpawnGoblins();
		return;
	}

	BeginDespawnSpawnedGoblins();
}

void ANPGoblinMapEvent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		DestroySpawnedGoblinsImmediately();
	}

	Super::EndPlay(EndPlayReason);
}

void ANPGoblinMapEvent::SpawnGoblins()
{
	DestroySpawnedGoblinsImmediately();

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

	const int32 TargetCount = FMath::Max(1, GoblinCount);
	const int32 AttemptsPerGoblin = FMath::Max(1, MaximumSpawnAttemptsPerGoblin);
	const FVector RequiredHalfExtent(
		FMath::Max(1.0f, GoblinRequiredHalfExtent.X),
		FMath::Max(1.0f, GoblinRequiredHalfExtent.Y),
		FMath::Max(1.0f, GoblinRequiredHalfExtent.Z));
	ANPGoblinPatrolRoute* PatrolRoute = FindPatrolRoute();
	if (!PatrolRoute)
	{
		UE_LOG(
			LogNPGoblinMapEvent,
			Warning,
			TEXT("사용 가능한 고블린 순찰 경로가 없어 랜덤 배회로 대체합니다. SpawnGroup=%s"),
			*GoblinSpawnGroup.ToString());
	}

	for (int32 GoblinIndex = 0; GoblinIndex < TargetCount; ++GoblinIndex)
	{
		bool bSpawned = false;
		for (int32 Attempt = 0; Attempt < AttemptsPerGoblin; ++Attempt)
		{
			FTransform GroundTransform;
			if (!EventManager->FindRandomSpawnTransformBySource(
					GoblinSpawnGroup,
					RequiredHalfExtent,
					GetLocationSource(),
					GroundTransform))
			{
				continue;
			}

			if (ANPGoblinCharacter* Goblin = SpawnGoblinAt(GroundTransform, PatrolRoute))
			{
				SpawnedGoblins.Add(Goblin);
				bSpawned = true;
				UE_LOG(
					LogNPGoblinMapEvent,
					Display,
					TEXT("고블린 생성 성공: Index=%d, Actor=%s, Location=%s"),
					GoblinIndex,
					*GetNameSafe(Goblin),
					*Goblin->GetActorLocation().ToCompactString());
				break;
			}
		}

		if (!bSpawned)
		{
			UE_LOG(
				LogNPGoblinMapEvent,
				Warning,
				TEXT("고블린 생성 실패: Index=%d, Attempts=%d"),
				GoblinIndex,
				AttemptsPerGoblin);
		}
	}

	UE_LOG(
		LogNPGoblinMapEvent,
		Display,
		TEXT("고블린 이벤트 생성 완료: Requested=%d, Spawned=%d"),
		TargetCount,
		SpawnedGoblins.Num());
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
	SpawnTransform.AddToTranslation(
		FVector::UpVector * FMath::Max(0.0f, GoblinSpawnHeightOffset));

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
	UGameplayStatics::FinishSpawningActor(Goblin, SpawnTransform);
	return Goblin;
}

void ANPGoblinMapEvent::BeginDespawnSpawnedGoblins()
{
	for (ANPGoblinCharacter* Goblin : SpawnedGoblins)
	{
		if (IsValid(Goblin))
		{
			Goblin->BeginDespawnPresentation();
		}
	}

	SpawnedGoblins.Reset();
}

void ANPGoblinMapEvent::DestroySpawnedGoblinsImmediately()
{
	for (ANPGoblinCharacter* Goblin : SpawnedGoblins)
	{
		if (IsValid(Goblin))
		{
			Goblin->Destroy();
		}
	}

	SpawnedGoblins.Reset();
}
