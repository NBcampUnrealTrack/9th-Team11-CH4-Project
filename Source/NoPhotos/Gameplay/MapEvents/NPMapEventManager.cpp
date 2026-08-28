#include "NPMapEventManager.h"

#include "Engine/World.h"
#include "Engine/LevelStreamingDynamic.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "NPMapEvent.h"
#include "NPMapEventCatalog.h"
#include "NPMapEventDefinition.h"
#include "NPMapEventLocationCollector.h"
#include "NPMapEventSpawnPoint.h"
#include "NPMapEventSpawnVolume.h"
#include "TimerManager.h"

UNPMapEventManagerComponent::UNPMapEventManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UNPMapEventManagerComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UNPMapEventManagerComponent, ActiveEventPresentations);
}

bool UNPMapEventManagerComponent::GetPrimaryActiveEventPresentation(
	FNPActiveMapEventPresentation& OutPresentation) const
{
	if (ActiveEventPresentations.IsEmpty())
	{
		OutPresentation = FNPActiveMapEventPresentation();
		return false;
	}

	OutPresentation = ActiveEventPresentations.Last();
	return true;
}

void UNPMapEventManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	CollectExistingLocationCollectors();
	if (!HasServerAuthority())
	{
		return;
	}

	CreateEventInstances();
	if (bStartAutomatically)
	{
		StartEventScheduling();
	}
}

void UNPMapEventManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopEventScheduling();
	for (ANPMapEvent* EventInstance : EventInstances)
	{
		if (IsValid(EventInstance))
		{
			EventInstance->OnEventStarted.RemoveDynamic(this, &ThisClass::HandleManagedEventStarted);
			EventInstance->OnEventFinished.RemoveDynamic(this, &ThisClass::HandleManagedEventFinished);
		}
	}
	if (IsValid(PendingEvent))
	{
		PendingEvent->OnEventFinished.RemoveDynamic(
			this,
			&ThisClass::HandleEventWithLocationLevelFinished);
	}
	if (IsValid(ActiveLocationLevel))
	{
		ActiveLocationLevel->OnLevelShown.RemoveDynamic(
			this,
			&ThisClass::HandleLocationLevelShown);
		ActiveLocationLevel->OnLevelUnloaded.RemoveDynamic(
			this,
			&ThisClass::HandleLocationLevelUnloaded);
	}
	PendingEvent = nullptr;
	ActiveLocationLevel = nullptr;
	bLocationLevelTransitionInProgress = false;
	LocationCollectors.Reset();
	EventLocationLevels.Reset();
	EventLocationLevelTransforms.Reset();
	EventInstances.Reset();
	Super::EndPlay(EndPlayReason);
}

void UNPMapEventManagerComponent::StartEventScheduling()
{
	if (!HasServerAuthority() || EventInstances.IsEmpty())
	{
		return;
	}

	StopEventScheduling();
	ScheduleEvent(ENPMapEventType::TypeA, TypeAMinimumTimeMinutes, TypeAMaximumTimeMinutes);
	ScheduleEvent(ENPMapEventType::TypeB, TypeBMinimumTimeMinutes, TypeBMaximumTimeMinutes);
}

void UNPMapEventManagerComponent::StopEventScheduling()
{
	if (HasServerAuthority())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(TypeAEventTimer);
			World->GetTimerManager().ClearTimer(TypeBEventTimer);
		}
	}
}

bool UNPMapEventManagerComponent::TriggerRandomEvent(const ENPMapEventType EventType)
{
	UWorld* World = GetWorld();
	if (!HasServerAuthority() || !World || (!bAllowConcurrentEvents && HasActiveEvent()))
	{
		return false;
	}

	TArray<ANPMapEvent*> Candidates;
	float TotalWeight = 0.0f;
	for (ANPMapEvent* EventInstance : EventInstances)
	{
		if (IsValid(EventInstance)
			&& EventInstance->CanRunAtEventTime(EventType)
			&& !EventInstance->RequiresStandaloneExecution()
			&& EventInstance->CanStartEvent())
		{
			Candidates.Add(EventInstance);
			TotalWeight += EventInstance->GetSelectionWeight();
		}
	}

	if (Candidates.IsEmpty() || TotalWeight <= 0.0f)
	{
		return false;
	}

	float Selection = FMath::FRandRange(0.0f, TotalWeight);
	for (ANPMapEvent* Candidate : Candidates)
	{
		Selection -= Candidate->GetSelectionWeight();
		if (Selection <= 0.0f)
		{
			return RequestEventStart(Candidate);
		}
	}

	return RequestEventStart(Candidates.Last());
}

void UNPMapEventManagerComponent::RegisterLocationCollector(
	ANPMapEventLocationCollector* Collector)
{
	if (!IsValid(Collector))
	{
		return;
	}

	LocationCollectors.AddUnique(Collector);
}

void UNPMapEventManagerComponent::UnregisterLocationCollector(
	ANPMapEventLocationCollector* Collector)
{
	LocationCollectors.Remove(Collector);
}

void UNPMapEventManagerComponent::GetSpawnVolumesForGroup(
	const FGameplayTag SpawnGroup,
	TArray<ANPMapEventSpawnVolume*>& OutSpawnVolumes) const
{
	OutSpawnVolumes.Reset();
	if (!HasServerAuthority() || !SpawnGroup.IsValid())
	{
		return;
	}

	for (const ANPMapEventLocationCollector* Collector : LocationCollectors)
	{
		if (!IsValid(Collector))
		{
			continue;
		}

		TArray<ANPMapEventSpawnVolume*> CollectorVolumes;
		Collector->GetSpawnVolumesForGroup(SpawnGroup, CollectorVolumes);
		for (ANPMapEventSpawnVolume* Volume : CollectorVolumes)
		{
			OutSpawnVolumes.AddUnique(Volume);
		}
	}
}

ANPMapEventSpawnPoint* UNPMapEventManagerComponent::FindRandomSpawnPoint(
	const FGameplayTag SpawnGroup) const
{
	if (!HasServerAuthority() || !SpawnGroup.IsValid())
	{
		return nullptr;
	}

	TArray<ANPMapEventSpawnPoint*> Candidates;
	for (ANPMapEventLocationCollector* Collector : LocationCollectors)
	{
		if (!IsValid(Collector))
		{
			continue;
		}

		TArray<ANPMapEventSpawnPoint*> CollectorPoints;
		Collector->GetSpawnPointsForGroup(SpawnGroup, CollectorPoints);
		for (ANPMapEventSpawnPoint* Point : CollectorPoints)
		{
			Candidates.AddUnique(Point);
		}
	}

	float TotalWeight = 0.0f;
	for (const ANPMapEventSpawnPoint* Candidate : Candidates)
	{
		TotalWeight += Candidate->GetSelectionWeight();
	}

	if (Candidates.IsEmpty() || TotalWeight <= 0.0f)
	{
		return nullptr;
	}

	float Selection = FMath::FRandRange(0.0f, TotalWeight);
	for (ANPMapEventSpawnPoint* Candidate : Candidates)
	{
		Selection -= Candidate->GetSelectionWeight();
		if (Selection <= 0.0f)
		{
			return Candidate;
		}
	}

	return Candidates.Last();
}

bool UNPMapEventManagerComponent::FindRandomSpawnTransform(
	const FGameplayTag SpawnGroup,
	const FVector RequiredHalfExtent,
	FTransform& OutTransform) const
{
	return FindRandomSpawnTransformBySource(
		SpawnGroup,
		RequiredHalfExtent,
		ENPMapEventLocationSource::Volume,
		OutTransform);
}

bool UNPMapEventManagerComponent::FindRandomSpawnTransformBySource(
	const FGameplayTag SpawnGroup,
	const FVector RequiredHalfExtent,
	const ENPMapEventLocationSource LocationSource,
	FTransform& OutTransform) const
{
	OutTransform = FTransform::Identity;
	if (!HasServerAuthority() || !SpawnGroup.IsValid())
	{
		return false;
	}

	struct FLocationCandidate
	{
		ANPMapEventSpawnPoint* Point = nullptr;
		ANPMapEventSpawnVolume* Volume = nullptr;
		float Weight = 0.0f;
	};

	TArray<FLocationCandidate> Candidates;
	for (ANPMapEventLocationCollector* Collector : LocationCollectors)
	{
		if (!IsValid(Collector))
		{
			continue;
		}

		if (LocationSource != ENPMapEventLocationSource::Volume)
		{
			TArray<ANPMapEventSpawnPoint*> CollectorPoints;
			Collector->GetSpawnPointsForGroup(SpawnGroup, CollectorPoints);
			for (ANPMapEventSpawnPoint* Point : CollectorPoints)
			{
				const bool bAlreadyAdded = Candidates.ContainsByPredicate(
					[Point](const FLocationCandidate& Candidate)
					{
						return Candidate.Point == Point;
					});
				if (!bAlreadyAdded)
				{
					FLocationCandidate& Candidate = Candidates.AddDefaulted_GetRef();
					Candidate.Point = Point;
					Candidate.Weight = Point->GetSelectionWeight();
				}
			}
		}

		if (LocationSource != ENPMapEventLocationSource::Point)
		{
			TArray<ANPMapEventSpawnVolume*> CollectorVolumes;
			Collector->GetSpawnVolumesForGroup(SpawnGroup, CollectorVolumes);
			for (ANPMapEventSpawnVolume* Volume : CollectorVolumes)
			{
				const bool bAlreadyAdded = Candidates.ContainsByPredicate(
					[Volume](const FLocationCandidate& Candidate)
					{
						return Candidate.Volume == Volume;
					});
				if (!bAlreadyAdded)
				{
					FLocationCandidate& Candidate = Candidates.AddDefaulted_GetRef();
					Candidate.Volume = Volume;
					Candidate.Weight = Volume->GetSelectionWeight();
				}
			}
		}
	}

	float TotalWeight = 0.0f;
	for (const FLocationCandidate& Candidate : Candidates)
	{
		TotalWeight += Candidate.Weight;
	}

	while (!Candidates.IsEmpty() && TotalWeight > 0.0f)
	{
		float Selection = FMath::FRandRange(0.0f, TotalWeight);
		int32 SelectedIndex = Candidates.Num() - 1;
		for (int32 Index = 0; Index < Candidates.Num(); ++Index)
		{
			Selection -= Candidates[Index].Weight;
			if (Selection <= 0.0f)
			{
				SelectedIndex = Index;
				break;
			}
		}

		const FLocationCandidate& SelectedCandidate = Candidates[SelectedIndex];
		if (IsValid(SelectedCandidate.Point))
		{
			OutTransform = SelectedCandidate.Point->GetActorTransform();
			return true;
		}
		if (IsValid(SelectedCandidate.Volume)
			&& SelectedCandidate.Volume->FindRandomGroundTransform(
				RequiredHalfExtent,
				OutTransform))
		{
			return true;
		}

		TotalWeight -= SelectedCandidate.Weight;
		Candidates.RemoveAtSwap(SelectedIndex, 1, EAllowShrinking::No);
	}

	return false;
}

bool UNPMapEventManagerComponent::HasServerAuthority() const
{
	const AActor* Owner = GetOwner();
	return IsValid(Owner) && Owner->HasAuthority();
}

void UNPMapEventManagerComponent::CollectExistingLocationCollectors(
	const ENPMapEventLocationSource LocationSource)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<ANPMapEventLocationCollector> Iterator(World); Iterator; ++Iterator)
	{
		ANPMapEventLocationCollector* Collector = *Iterator;
		if (IsValid(Collector))
		{
			Collector->RefreshLocations(LocationSource);
			RegisterLocationCollector(Collector);
		}
	}
}

void UNPMapEventManagerComponent::CreateEventInstances()
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!HasServerAuthority() || !World || !Owner || !EventCatalog)
	{
		return;
	}

	const FTransform SpawnTransform = Owner->GetActorTransform();
	for (const FNPMapEventCatalogEntry& Entry : EventCatalog->GetEventEntries())
	{
		UNPMapEventDefinition* Definition = Entry.EventDefinition;
		const TSubclassOf<ANPMapEvent> EventClass = Definition
			? Definition->GetEventClass()
			: nullptr;
		if (!EventClass)
		{
			continue;
		}

		ANPMapEvent* EventInstance = World->SpawnActorDeferred<ANPMapEvent>(
			EventClass,
			SpawnTransform,
			Owner,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (EventInstance)
		{
			EventInstance->InitializeEvent(Definition, Entry.SelectionWeight);
			RegisterManagedEvent(EventInstance);
			EventInstance->FinishSpawning(SpawnTransform);
			if (!Entry.LocationLevelInstance.IsNull())
			{
				EventLocationLevels.Add(EventInstance, Entry.LocationLevelInstance);
				EventLocationLevelTransforms.Add(EventInstance, Entry.LocationLevelTransform);
			}
		}
	}

	// 기존 카탈로그 에셋을 EventEntries로 옮기는 동안만 유지하는 호환 경로입니다.
	if (!EventCatalog->GetEventEntries().IsEmpty())
	{
		return;
	}

	for (const TSubclassOf<ANPMapEvent>& EventClass : EventCatalog->GetEventClasses())
	{
		if (!EventClass)
		{
			continue;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = Owner;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (ANPMapEvent* EventInstance = World->SpawnActor<ANPMapEvent>(EventClass, SpawnTransform, SpawnParameters))
		{
			RegisterManagedEvent(EventInstance);
		}
	}
}

void UNPMapEventManagerComponent::RegisterManagedEvent(ANPMapEvent* EventInstance)
{
	if (!IsValid(EventInstance))
	{
		return;
	}

	EventInstances.AddUnique(EventInstance);
	EventInstance->OnEventStarted.AddUniqueDynamic(this, &ThisClass::HandleManagedEventStarted);
	EventInstance->OnEventFinished.AddUniqueDynamic(this, &ThisClass::HandleManagedEventFinished);

	// SpawnActor 경로에서 BeginPlay 중 이미 시작된 이벤트도 놓치지 않습니다.
	if (EventInstance->IsEventActive())
	{
		HandleManagedEventStarted(EventInstance);
	}
}

void UNPMapEventManagerComponent::HandleManagedEventStarted(ANPMapEvent* MapEvent)
{
	if (!HasServerAuthority() || !IsValid(MapEvent))
	{
		return;
	}

	FNPActiveMapEventPresentation Presentation;
	Presentation.EventId = MapEvent->GetEventId();
	if (Presentation.EventId.IsNone())
	{
		Presentation.EventId = MapEvent->GetFName();
	}
	Presentation.Title = MapEvent->GetEventDisplayName();
	Presentation.Description = MapEvent->GetEventDescription();
	Presentation.EndServerWorldTime = MapEvent->GetEventEndServerWorldTime();
	Presentation.DurationSeconds = MapEvent->GetEventDuration();

	ActiveEventPresentations.RemoveAll(
		[EventId = Presentation.EventId](const FNPActiveMapEventPresentation& Existing)
		{
			return Existing.EventId == EventId;
		});
	ActiveEventPresentations.Add(MoveTemp(Presentation));
	NotifyActiveEventPresentationsChanged();
}

void UNPMapEventManagerComponent::HandleManagedEventFinished(ANPMapEvent* MapEvent)
{
	if (!HasServerAuthority() || !IsValid(MapEvent))
	{
		return;
	}

	FName EventId = MapEvent->GetEventId();
	if (EventId.IsNone())
	{
		EventId = MapEvent->GetFName();
	}

	const int32 RemovedCount = ActiveEventPresentations.RemoveAll(
		[EventId](const FNPActiveMapEventPresentation& Existing)
		{
			return Existing.EventId == EventId;
		});
	if (RemovedCount > 0)
	{
		NotifyActiveEventPresentationsChanged();
	}
}

void UNPMapEventManagerComponent::OnRep_ActiveEventPresentations()
{
	OnActiveMapEventsChanged.Broadcast();
}

void UNPMapEventManagerComponent::NotifyActiveEventPresentationsChanged()
{
	OnActiveMapEventsChanged.Broadcast();
	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}
}

bool UNPMapEventManagerComponent::RequestEventStart(ANPMapEvent* EventInstance)
{
	if (!HasServerAuthority()
		|| !IsValid(EventInstance)
		|| bLocationLevelTransitionInProgress
		|| IsValid(ActiveLocationLevel))
	{
		return false;
	}

	const TSoftObjectPtr<UWorld>* LocationLevelInstance = EventLocationLevels.Find(EventInstance);
	if (!LocationLevelInstance || LocationLevelInstance->IsNull())
	{
		CollectExistingLocationCollectors(EventInstance->GetLocationSource());
		return EventInstance->StartEvent();
	}

	const FTransform* LocationLevelTransform = EventLocationLevelTransforms.Find(EventInstance);
	return LoadEventLocationLevel(
		EventInstance,
		*LocationLevelInstance,
		LocationLevelTransform ? *LocationLevelTransform : FTransform::Identity);
}

bool UNPMapEventManagerComponent::LoadEventLocationLevel(
	ANPMapEvent* EventInstance,
	const TSoftObjectPtr<UWorld>& LocationLevelInstance,
	const FTransform& LocationLevelTransform)
{
	if (!HasServerAuthority()
		|| !IsValid(EventInstance)
		|| LocationLevelInstance.IsNull()
		|| bLocationLevelTransitionInProgress
		|| IsValid(ActiveLocationLevel))
	{
		return false;
	}

	bool bLoadSucceeded = false;
	const FString InstanceName = FString::Printf(
		TEXT("NP_MapEventLocation_%d"),
		++LocationLevelInstanceSerial);
	ULevelStreamingDynamic* LoadedLevel = ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(
		this,
		LocationLevelInstance,
		LocationLevelTransform,
		bLoadSucceeded,
		InstanceName);
	if (!bLoadSucceeded || !IsValid(LoadedLevel))
	{
		return false;
	}

	PendingEvent = EventInstance;
	ActiveLocationLevel = LoadedLevel;
	bLocationLevelTransitionInProgress = true;
	LoadedLevel->OnLevelShown.AddUniqueDynamic(this, &ThisClass::HandleLocationLevelShown);
	LoadedLevel->OnLevelUnloaded.AddUniqueDynamic(this, &ThisClass::HandleLocationLevelUnloaded);

	// 이미 표시된 레벨 에셋이 재사용되는 특수한 경우에도 이벤트 시작을 놓치지 않습니다.
	if (LoadedLevel->IsLevelVisible())
	{
		HandleLocationLevelShown();
	}

	return true;
}

void UNPMapEventManagerComponent::HandleLocationLevelShown()
{
	if (!HasServerAuthority()
		|| !bLocationLevelTransitionInProgress
		|| !IsValid(ActiveLocationLevel))
	{
		return;
	}

	ActiveLocationLevel->OnLevelShown.RemoveDynamic(this, &ThisClass::HandleLocationLevelShown);
	bLocationLevelTransitionInProgress = false;

	ANPMapEvent* EventToStart = PendingEvent;
	PendingEvent = nullptr;
	if (!IsValid(EventToStart))
	{
		UnloadActiveLocationLevel();
		return;
	}

	// 이벤트 BP가 선택한 Point/Volume 종류만 다시 수집합니다.
	// 스트리밍된 Level Instance 내부 Collector의 BeginPlay 순서에도 의존하지 않습니다.
	CollectExistingLocationCollectors(EventToStart->GetLocationSource());

	EventToStart->OnEventFinished.AddUniqueDynamic(
		this,
		&ThisClass::HandleEventWithLocationLevelFinished);
	if (!EventToStart->StartEvent())
	{
		EventToStart->OnEventFinished.RemoveDynamic(
			this,
			&ThisClass::HandleEventWithLocationLevelFinished);
		UnloadActiveLocationLevel();
	}
}

void UNPMapEventManagerComponent::HandleEventWithLocationLevelFinished(ANPMapEvent* MapEvent)
{
	if (IsValid(MapEvent))
	{
		MapEvent->OnEventFinished.RemoveDynamic(
			this,
			&ThisClass::HandleEventWithLocationLevelFinished);
	}

	UnloadActiveLocationLevel();
}

void UNPMapEventManagerComponent::UnloadActiveLocationLevel()
{
	if (!IsValid(ActiveLocationLevel))
	{
		PendingEvent = nullptr;
		bLocationLevelTransitionInProgress = false;
		return;
	}

	bLocationLevelTransitionInProgress = true;
	ActiveLocationLevel->SetShouldBeVisible(false);
	ActiveLocationLevel->SetShouldBeLoaded(false);
}

void UNPMapEventManagerComponent::HandleLocationLevelUnloaded()
{
	if (IsValid(ActiveLocationLevel))
	{
		ActiveLocationLevel->OnLevelShown.RemoveDynamic(
			this,
			&ThisClass::HandleLocationLevelShown);
		ActiveLocationLevel->OnLevelUnloaded.RemoveDynamic(
			this,
			&ThisClass::HandleLocationLevelUnloaded);
	}

	PendingEvent = nullptr;
	ActiveLocationLevel = nullptr;
	bLocationLevelTransitionInProgress = false;
	LocationCollectors.RemoveAll(
		[](const ANPMapEventLocationCollector* Collector)
		{
			return !IsValid(Collector);
		});
}

void UNPMapEventManagerComponent::ScheduleEvent(
	const ENPMapEventType EventType,
	const float MinimumTimeMinutes,
	const float MaximumTimeMinutes)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float LowerBoundMinutes = FMath::Max(0.0f, FMath::Min(MinimumTimeMinutes, MaximumTimeMinutes));
	const float UpperBoundMinutes = FMath::Max(0.0f, FMath::Max(MinimumTimeMinutes, MaximumTimeMinutes));
	const float DelaySeconds = FMath::FRandRange(LowerBoundMinutes, UpperBoundMinutes) * 60.0f;
	FTimerHandle& TimerHandle = EventType == ENPMapEventType::TypeA
		? TypeAEventTimer
		: TypeBEventTimer;
	void (UNPMapEventManagerComponent::*TimerCallback)() = EventType == ENPMapEventType::TypeA
		? &UNPMapEventManagerComponent::HandleTypeAEventTimer
		: &UNPMapEventManagerComponent::HandleTypeBEventTimer;

	World->GetTimerManager().SetTimer(
		TimerHandle,
		this,
		TimerCallback,
		FMath::Max(DelaySeconds, 0.01f),
		false);
}

void UNPMapEventManagerComponent::HandleTypeAEventTimer()
{
	TriggerRandomEvent(ENPMapEventType::TypeA);
}

void UNPMapEventManagerComponent::HandleTypeBEventTimer()
{
	TriggerRandomEvent(ENPMapEventType::TypeB);
}

bool UNPMapEventManagerComponent::HasActiveEvent() const
{
	return bLocationLevelTransitionInProgress
		|| IsValid(ActiveLocationLevel)
		|| EventInstances.ContainsByPredicate(
		[](const ANPMapEvent* EventInstance)
		{
			return IsValid(EventInstance) && EventInstance->IsEventActive();
		});
}
