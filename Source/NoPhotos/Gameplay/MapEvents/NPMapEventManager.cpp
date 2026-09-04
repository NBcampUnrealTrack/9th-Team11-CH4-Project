#include "NPMapEventManager.h"

#include "Engine/World.h"
#include "Core/Main/NPMainGameState.h"
#include "Engine/LevelStreamingDynamic.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "NPMapEvent.h"
#include "NPMapEventCatalog.h"
#include "NPMapEventDefinition.h"
#include "NPMapEventLocationCollector.h"
#include "NPMapEventSpawnPoint.h"
#include "NPMapEventSpawnVolume.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogNPMapEventManager, Log, All);

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
	DOREPLIFETIME(UNPMapEventManagerComponent, EventSchedule);
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

bool UNPMapEventManagerComponent::GetNextScheduledEventPresentation(
	FNPScheduledMapEventPresentation& OutPresentation) const
{
	for (const FNPScheduledMapEventPresentation& Entry : EventSchedule.Events)
	{
		if (Entry.State == ENPScheduledMapEventState::Pending || Entry.State == ENPScheduledMapEventState::Loading)
		{
			OutPresentation = Entry;
			return true;
		}
	}
	OutPresentation = FNPScheduledMapEventPresentation();
	return false;
}

float UNPMapEventManagerComponent::GetServerWorldTimeSeconds() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0f);
}

float UNPMapEventManagerComponent::GetNextEventStartRemainingSeconds() const
{
	FNPScheduledMapEventPresentation NextEvent;
	return GetNextScheduledEventPresentation(NextEvent) && NextEvent.ExpectedStartServerWorldTime >= 0.0f
		? FMath::Max(0.0f, NextEvent.ExpectedStartServerWorldTime - GetServerWorldTimeSeconds()) : -1.0f;
}

float UNPMapEventManagerComponent::GetScheduleElapsedSeconds() const
{
	return FMath::Max(0.0f, GetServerWorldTimeSeconds() - EventSchedule.ScheduleOriginServerWorldTime);
}

void UNPMapEventManagerComponent::UpdateExpectedEventTimes(const int32 FirstIndex, float FirstStartServerWorldTime)
{
	for (int32 Index = FirstIndex; Index < EventSchedule.Events.Num(); ++Index)
	{
		FNPScheduledMapEventPresentation& Entry = EventSchedule.Events[Index];
		if (Entry.State != ENPScheduledMapEventState::Pending)
		{
			continue;
		}
		Entry.ExpectedStartServerWorldTime = FirstStartServerWorldTime;
		Entry.ExpectedEndServerWorldTime = FirstStartServerWorldTime >= 0.0f && Entry.DurationSeconds > 0.0f
			? FirstStartServerWorldTime + Entry.DurationSeconds : -1.0f;
		// Duration=0은 무기한 이벤트입니다. 실제 종료 전까지 후속 시작 시각을 확정하지 않습니다.
		FirstStartServerWorldTime = Entry.ExpectedEndServerWorldTime >= 0.0f
			? Entry.ExpectedEndServerWorldTime + Entry.DelayAfterSeconds : -1.0f;
	}
}

void UNPMapEventManagerComponent::OnRep_EventSchedule()
{
	OnEventScheduleChanged.Broadcast();
}

void UNPMapEventManagerComponent::NotifyEventScheduleChanged()
{
	OnEventScheduleChanged.Broadcast();
	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}
}

void UNPMapEventManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	CollectExistingLocationCollectors();
	if (!HasServerAuthority())
	{
		return;
	}

	ScheduleOriginServerWorldTime = GetServerWorldTimeSeconds();
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
			EventInstance->OnEventFinished.RemoveDynamic(this, &ThisClass::HandleEventWithLocationLevelFinished);
			EventInstance->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleManagedEventDestroyed);
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
	PlannedEvents.Reset();
	Super::EndPlay(EndPlayReason);
}

void UNPMapEventManagerComponent::StartEventScheduling()
{
	if (!HasServerAuthority() || IsGameOver() || !GetWorld() || bScheduling || ScheduledEvent)
	{
		return;
	}

	// 미리 만들고 복제하는 배열이므로 잘못 입력된 대량 실행 개수를 제한합니다.
	constexpr int32 MaximumPlanSize = 256;
	const int32 LowerBound = FMath::Clamp(FMath::Min(MinimumEventCount, MaximumEventCount), 0, MaximumPlanSize);
	const int32 UpperBound = FMath::Clamp(FMath::Max(MinimumEventCount, MaximumEventCount), LowerBound, MaximumPlanSize);
	TargetEventCount = LowerBound == UpperBound ? LowerBound : FMath::RandRange(LowerBound, UpperBound);
	StartedEventCount = 0;
	ScheduledPlanIndex = INDEX_NONE;
	PlannedEvents.Reset();
	EventSchedule = FNPMapEventSchedulePresentation();
	if (ScheduleOriginServerWorldTime < 0.0f)
	{
		ScheduleOriginServerWorldTime = GetServerWorldTimeSeconds();
	}
	EventSchedule.ScheduleOriginServerWorldTime = ScheduleOriginServerWorldTime;
	for (int32 Index = 0; Index < TargetEventCount; ++Index)
	{
		// 지금 수동 실행 중이어도 나중에 다시 실행할 수 있으므로 계획에서는 활성 여부를 제외합니다.
		ANPMapEvent* Candidate = SelectRandomEvent(nullptr, false, true);
		if (!Candidate)
		{
			UE_LOG(LogNPMapEventManager, Display,
				TEXT("[MapEventPlan] 중복 제외 후 후보 부족: Requested=%d Available=%d (중복 없이 개수 축소)"),
				TargetEventCount, PlannedEvents.Num());
			break;
		}
		PlannedEvents.Add(Candidate);
		FNPScheduledMapEventPresentation& Entry = EventSchedule.Events.AddDefaulted_GetRef();
		Entry.ScheduleIndex = Index;
		Entry.EventId = Candidate->GetEventId().IsNone() ? Candidate->GetFName() : Candidate->GetEventId();
		Entry.Title = Candidate->GetEventDisplayName();
		Entry.Description = Candidate->GetEventDescription();
		Entry.DurationSeconds = Candidate->GetEventDuration();
		const UNPMapEventDefinition* Definition = Candidate->GetEventDefinition();
		Entry.DelayAfterSeconds = Definition ? Definition->GetDelay() : 0.0f;
	}
	TargetEventCount = PlannedEvents.Num();
	bScheduling = TargetEventCount > 0;
	EventSchedule.bRunning = bScheduling;
	const float FirstStartTime = FMath::Max(GetServerWorldTimeSeconds(),
		ScheduleOriginServerWorldTime + FMath::Max(0.0f, FirstEventStartTimeSeconds));
	UpdateExpectedEventTimes(0, FirstStartTime);
	UE_LOG(LogNPMapEventManager, Display,
		TEXT("[MapEventPlan] 자동 이벤트 계획 확정: Manager=%s Count=%d (시간/Delay와 관계없이 이 개수까지만 실행)"),
		*GetPathName(), TargetEventCount);
	for (const FNPScheduledMapEventPresentation& Entry : EventSchedule.Events)
	{
		const FString EventName = Entry.Title.IsEmpty() ? Entry.EventId.ToString() : Entry.Title.ToString();
		const FString ExpectedStart = Entry.ExpectedStartServerWorldTime >= 0.0f
			? FString::Printf(TEXT("게임 시작 후 %.2f초"),
				Entry.ExpectedStartServerWorldTime - EventSchedule.ScheduleOriginServerWorldTime)
			: TEXT("앞 이벤트 종료 후 확정");
		UE_LOG(LogNPMapEventManager, Display,
			TEXT("[MapEventPlan] [%d/%d] 예정 이벤트=%s EventId=%s | 예상 시작=%s | Duration=%.2f초 | 종료 후 Delay=%.2f초"),
			Entry.ScheduleIndex + 1, TargetEventCount, *EventName, *Entry.EventId.ToString(),
			*ExpectedStart, Entry.DurationSeconds, Entry.DelayAfterSeconds);
	}
	ScheduleNextEvent(FirstStartTime - GetServerWorldTimeSeconds());
	NotifyEventScheduleChanged();
}

void UNPMapEventManagerComponent::StopEventScheduling()
{
	if (!HasServerAuthority())
	{
		return;
	}

	bScheduling = false;
	EventSchedule.bRunning = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(NextEventTimer);
	}
	if (ScheduledEvent && PendingEvent == ScheduledEvent)
	{
		// 레벨 표시 콜백이 나중에 도착해도 취소한 자동 이벤트를 시작하지 않습니다.
		PendingEvent = nullptr;
		UnloadActiveLocationLevel();
	}
	for (FNPScheduledMapEventPresentation& Entry : EventSchedule.Events)
	{
		if (Entry.State == ENPScheduledMapEventState::Pending || Entry.State == ENPScheduledMapEventState::Loading)
		{
			Entry.State = ENPScheduledMapEventState::Cancelled;
		}
	}
	// 이미 시작한 이벤트는 계속 실행되므로 종료 시각과 Completed 상태를 이후에도 반영합니다.
	if (!bScheduledEventStarted)
	{
		ScheduledEvent = nullptr;
		ScheduledPlanIndex = INDEX_NONE;
	}
	NotifyEventScheduleChanged();
}

void UNPMapEventManagerComponent::ShutdownEventsForGameEnd()
{
	if (!HasServerAuthority() || bEventsShutdown)
	{
		return;
	}
	// 종료 콜백이나 BP가 다시 실행을 요청하기 전에 먼저 차단합니다.
	bEventsShutdown = true;
	PendingEvent = nullptr; // 수동 이벤트의 비동기 로드 대기도 취소합니다.
	StopEventScheduling();
	const TArray<TObjectPtr<ANPMapEvent>> EventsToFinish = EventInstances;
	for (ANPMapEvent* MapEvent : EventsToFinish)
	{
		if (IsValid(MapEvent) && MapEvent->IsEventActive())
		{
			MapEvent->FinishEvent();
		}
	}
	UnloadActiveLocationLevel();
	ActiveEventPresentations.Reset();
	NotifyActiveEventPresentationsChanged();
	NotifyEventScheduleChanged();
	UE_LOG(LogNPMapEventManager, Display,
		TEXT("[MapEventPlan] 게임 종료: 진행 이벤트 강제 종료, 대기/로딩 취소, 이후 실행 차단. Manager=%s"), *GetPathName());
}

bool UNPMapEventManagerComponent::TriggerRandomEvent(const ENPMapEventType EventType)
{
	UWorld* World = GetWorld();
	if (!HasServerAuthority() || IsGameOver() || !World || (!bAllowConcurrentEvents && HasActiveEvent()))
	{
		return false;
	}

	ANPMapEvent* Candidate = SelectRandomEvent(&EventType, true, bScheduling);
	return Candidate && RequestEventStart(Candidate);
}

FString UNPMapEventManagerComponent::GetEventIdentityKey(const ANPMapEvent* MapEvent)
{
	if (!MapEvent->GetEventId().IsNone())
	{
		return TEXT("Id:") + MapEvent->GetEventId().ToString();
	}
	if (const UNPMapEventDefinition* Definition = MapEvent->GetEventDefinition())
	{
		return TEXT("Definition:") + Definition->GetPathName();
	}
	return TEXT("Class:") + MapEvent->GetClass()->GetPathName();
}

bool UNPMapEventManagerComponent::IsGameOver() const
{
	const ANPMainGameState* MainState = Cast<ANPMainGameState>(GetOwner());
	if (!MainState && GetWorld())
	{
		MainState = GetWorld()->GetGameState<ANPMainGameState>();
	}
	return bEventsShutdown || (MainState && MainState->IsMainGameEnded());
}

ANPMapEvent* UNPMapEventManagerComponent::SelectRandomEvent(
	const ENPMapEventType* EventType, const bool bRequireReady, const bool bExcludePlanned) const
{
	TArray<ANPMapEvent*> Candidates;
	float TotalWeight = 0.0f;
	for (ANPMapEvent* EventInstance : EventInstances)
	{
		if (!IsValid(EventInstance))
		{
			continue;
		}
		const FString Identity = GetEventIdentityKey(EventInstance);
		if (PlayedEventKeys.Contains(Identity) || (bExcludePlanned && PlannedEvents.ContainsByPredicate(
			[&Identity](const ANPMapEvent* Planned)
			{
				return IsValid(Planned) && GetEventIdentityKey(Planned) == Identity;
			})))
		{
			continue;
		}
		if (IsValid(EventInstance)
			&& (!EventType || EventInstance->CanRunAtEventTime(*EventType))
			&& !EventInstance->RequiresStandaloneExecution()
			&& EventInstance->GetSelectionWeight() > 0.0f
			&& (!bRequireReady || EventInstance->CanStartEvent()))
		{
			Candidates.Add(EventInstance);
			TotalWeight += EventInstance->GetSelectionWeight();
		}
	}

	if (Candidates.IsEmpty() || TotalWeight <= 0.0f)
	{
		return nullptr;
	}

	float Selection = FMath::FRandRange(0.0f, TotalWeight);
	for (ANPMapEvent* Candidate : Candidates)
	{
		Selection -= Candidate->GetSelectionWeight();
		if (Selection <= 0.0f)
		{
			UE_LOG(LogNPMapEventManager, Display,
				TEXT("[MapEventTrace] 랜덤 이벤트 선택: Type=%d Event=%s Class=%s EventId=%s Weight=%.2f"),
				EventType ? static_cast<int32>(*EventType) : -1, *GetNameSafe(Candidate),
				*GetNameSafe(Candidate->GetClass()), *Candidate->GetEventId().ToString(),
				Candidate->GetSelectionWeight());
			return Candidate;
		}
	}

	UE_LOG(LogNPMapEventManager, Display,
		TEXT("[MapEventTrace] 랜덤 이벤트 최종 후보 선택: Type=%d Event=%s Class=%s EventId=%s"),
		EventType ? static_cast<int32>(*EventType) : -1, *GetNameSafe(Candidates.Last()),
		*GetNameSafe(Candidates.Last()->GetClass()), *Candidates.Last()->GetEventId().ToString());
	return Candidates.Last();
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
			UE_LOG(LogNPMapEventManager, Display,
				TEXT("[MapEventTrace] 카탈로그 이벤트 인스턴스 생성: Definition=%s EventId=%s Actor=%s Class=%s Weight=%.2f (아직 시작되지 않음)"),
				*GetNameSafe(Definition), *EventInstance->GetEventId().ToString(),
				*GetNameSafe(EventInstance), *GetNameSafe(EventInstance->GetClass()),
				Entry.SelectionWeight);
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
	EventInstance->OnDestroyed.AddUniqueDynamic(this, &ThisClass::HandleManagedEventDestroyed);

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

	if (IsGameOver())
	{
		MapEvent->FinishEvent();
		return;
	}
	PlayedEventKeys.Add(GetEventIdentityKey(MapEvent));
	if (bScheduling && ScheduledEvent == MapEvent && !bScheduledEventStarted)
	{
		bScheduledEventStarted = true;
		++StartedEventCount;
		if (EventSchedule.Events.IsValidIndex(ScheduledPlanIndex))
		{
			FNPScheduledMapEventPresentation& Entry = EventSchedule.Events[ScheduledPlanIndex];
			Entry.State = ENPScheduledMapEventState::Active;
			Entry.ActualStartServerWorldTime = GetServerWorldTimeSeconds();
			Entry.ExpectedStartServerWorldTime = Entry.ActualStartServerWorldTime;
			Entry.DurationSeconds = MapEvent->GetEventDuration();
			Entry.ExpectedEndServerWorldTime = Entry.DurationSeconds > 0.0f ? MapEvent->GetEventEndServerWorldTime() : -1.0f;
			UpdateExpectedEventTimes(ScheduledPlanIndex + 1, Entry.ExpectedEndServerWorldTime >= 0.0f
				? Entry.ExpectedEndServerWorldTime + Entry.DelayAfterSeconds : -1.0f);
		}
		UE_LOG(LogNPMapEventManager, Display, TEXT("[MapEventTrace] 자동 이벤트 시작: %d/%d Event=%s"),
			StartedEventCount, TargetEventCount, *GetNameSafe(MapEvent));
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
	NotifyEventScheduleChanged();
}

void UNPMapEventManagerComponent::HandleManagedEventFinished(ANPMapEvent* MapEvent)
{
	if (!HasServerAuthority() || !IsValid(MapEvent))
	{
		return;
	}

	if (ScheduledEvent == MapEvent)
	{
		float DelaySeconds = 0.0f;
		if (EventSchedule.Events.IsValidIndex(ScheduledPlanIndex))
		{
			FNPScheduledMapEventPresentation& Entry = EventSchedule.Events[ScheduledPlanIndex];
			Entry.State = bEventsShutdown ? ENPScheduledMapEventState::Cancelled : ENPScheduledMapEventState::Completed;
			Entry.ActualEndServerWorldTime = GetServerWorldTimeSeconds();
			Entry.ExpectedEndServerWorldTime = Entry.ActualEndServerWorldTime;
			DelaySeconds = Entry.DelayAfterSeconds;
			UpdateExpectedEventTimes(ScheduledPlanIndex + 1, Entry.ActualEndServerWorldTime + DelaySeconds);
		}
		ScheduledEvent = nullptr;
		ScheduledPlanIndex = INDEX_NONE;
		bScheduledEventStarted = false;
		if (StartedEventCount >= TargetEventCount)
		{
			bScheduling = false;
			EventSchedule.bRunning = false;
			if (!bEventsShutdown)
			{
				UE_LOG(LogNPMapEventManager, Display,
					TEXT("[MapEventPlan] 자동 이벤트 계획 완료: Manager=%s Executed=%d/%d (추가 자동 실행 없음)"),
					*GetPathName(), StartedEventCount, TargetEventCount);
			}
		}
		else if (bScheduling)
		{
			ScheduleNextEvent(DelaySeconds);
		}
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
	NotifyEventScheduleChanged();
}

void UNPMapEventManagerComponent::HandleManagedEventDestroyed(AActor* DestroyedActor)
{
	ANPMapEvent* MapEvent = Cast<ANPMapEvent>(DestroyedActor);
	if (!HasServerAuthority() || !MapEvent)
	{
		return;
	}

	if (ScheduledEvent == MapEvent)
	{
		if (EventSchedule.Events.IsValidIndex(ScheduledPlanIndex))
		{
			FNPScheduledMapEventPresentation& Entry = EventSchedule.Events[ScheduledPlanIndex];
			Entry.State = ENPScheduledMapEventState::Cancelled;
			Entry.ActualEndServerWorldTime = GetServerWorldTimeSeconds();
		}
		bScheduledEventStarted = false;
		UE_LOG(LogNPMapEventManager, Warning, TEXT("자동 이벤트가 파괴되어 스케줄을 중지합니다: %s"), *GetNameSafe(MapEvent));
		StopEventScheduling();
	}
	if (PendingEvent == MapEvent || (MapEvent->IsEventActive() && EventLocationLevels.Contains(MapEvent)))
	{
		PendingEvent = nullptr;
		UnloadActiveLocationLevel();
	}
	const FName EventId = MapEvent->GetEventId().IsNone() ? MapEvent->GetFName() : MapEvent->GetEventId();
	if (ActiveEventPresentations.RemoveAll([EventId](const FNPActiveMapEventPresentation& Entry)
		{ return Entry.EventId == EventId; }) > 0)
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
	UE_LOG(LogNPMapEventManager, Display,
		TEXT("[MapEventTrace] 이벤트 시작 요청: Event=%s Class=%s EventId=%s Authority=%d Transition=%d ActiveLocation=%s"),
		*GetNameSafe(EventInstance), *GetNameSafe(EventInstance ? EventInstance->GetClass() : nullptr),
		EventInstance ? *EventInstance->GetEventId().ToString() : TEXT("None"),
		HasServerAuthority() ? 1 : 0, bLocationLevelTransitionInProgress ? 1 : 0,
		*GetNameSafe(ActiveLocationLevel));
	if (!HasServerAuthority()
		|| !IsValid(EventInstance)
		|| IsGameOver()
		|| PlayedEventKeys.Contains(GetEventIdentityKey(EventInstance))
		|| bLocationLevelTransitionInProgress
		|| IsValid(ActiveLocationLevel))
	{
		return false;
	}

	const TSoftObjectPtr<UWorld>* LocationLevelInstance = EventLocationLevels.Find(EventInstance);
	if (!LocationLevelInstance || LocationLevelInstance->IsNull())
	{
		CollectExistingLocationCollectors(EventInstance->GetLocationSource());
		const bool bStarted = EventInstance->StartEvent();
		UE_LOG(LogNPMapEventManager, Display,
			TEXT("[MapEventTrace] 위치 레벨 없이 StartEvent 완료: Event=%s Class=%s Result=%d Active=%d"),
			*GetNameSafe(EventInstance), *GetNameSafe(EventInstance->GetClass()),
			bStarted ? 1 : 0, EventInstance->IsEventActive() ? 1 : 0);
		return bStarted;
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
		|| IsGameOver()
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
	if (HasServerAuthority() && IsGameOver())
	{
		PendingEvent = nullptr;
		UnloadActiveLocationLevel();
		return;
	}
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
	if (PlayedEventKeys.Contains(GetEventIdentityKey(EventToStart)))
	{
		if (ScheduledEvent == EventToStart)
		{
			StopEventScheduling();
		}
		UnloadActiveLocationLevel();
		return;
	}

	// 이벤트 BP가 선택한 Point/Volume 종류만 다시 수집합니다.
	// 스트리밍된 Level Instance 내부 Collector의 BeginPlay 순서에도 의존하지 않습니다.
	CollectExistingLocationCollectors(EventToStart->GetLocationSource());

	EventToStart->OnEventFinished.AddUniqueDynamic(
		this,
		&ThisClass::HandleEventWithLocationLevelFinished);
	UE_LOG(LogNPMapEventManager, Display,
		TEXT("[MapEventTrace] 위치 레벨 표시 후 StartEvent 호출: Event=%s Class=%s EventId=%s Source=%d"),
		*GetNameSafe(EventToStart), *GetNameSafe(EventToStart->GetClass()),
		*EventToStart->GetEventId().ToString(), static_cast<int32>(EventToStart->GetLocationSource()));
	const bool bStarted = EventToStart->StartEvent();
	UE_LOG(LogNPMapEventManager, Display,
		TEXT("[MapEventTrace] 위치 레벨 StartEvent 완료: Event=%s Class=%s Result=%d Active=%d"),
		*GetNameSafe(EventToStart), *GetNameSafe(EventToStart->GetClass()),
		bStarted ? 1 : 0, EventToStart->IsEventActive() ? 1 : 0);
	if (!bStarted)
	{
		if (bScheduling && ScheduledEvent == EventToStart)
		{
			UE_LOG(LogNPMapEventManager, Warning, TEXT("위치 레벨 로드 후 자동 이벤트 시작에 실패하여 스케줄을 중지합니다."));
			StopEventScheduling();
		}
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

void UNPMapEventManagerComponent::ScheduleNextEvent(const float DelaySeconds)
{
	UWorld* World = GetWorld();
	if (!bScheduling || IsGameOver() || !World)
	{
		return;
	}

	// 0초도 비동기로 처리해 종료 델리게이트와 위치 레벨 정리가 먼저 완료되게 합니다.
	World->GetTimerManager().SetTimer(
		NextEventTimer,
		this,
		&ThisClass::HandleNextEventTimer,
		FMath::Max(DelaySeconds, 0.01f),
		false);
}

void UNPMapEventManagerComponent::HandleNextEventTimer()
{
	if (!HasServerAuthority() || IsGameOver() || !bScheduling || ScheduledEvent)
	{
		return;
	}
	if (StartedEventCount >= TargetEventCount)
	{
		StopEventScheduling();
		return;
	}
	if (HasActiveEvent())
	{
		// Delay가 끝나도 이전 레벨 언로드 또는 수동 이벤트가 끝날 때까지 기다립니다.
		ScheduleNextEvent(0.1f);
		return;
	}

	// UI에 이미 공개한 순서를 그대로 실행합니다. 실패해도 다른 이벤트로 몰래 재추첨하지 않습니다.
	ScheduledPlanIndex = StartedEventCount;
	ScheduledEvent = PlannedEvents.IsValidIndex(ScheduledPlanIndex) ? PlannedEvents[ScheduledPlanIndex] : nullptr;
	bScheduledEventStarted = false;
	if (IsValid(ScheduledEvent) && EventSchedule.Events.IsValidIndex(ScheduledPlanIndex))
	{
		EventSchedule.Events[ScheduledPlanIndex].State = ENPScheduledMapEventState::Loading;
	}
	if (!IsValid(ScheduledEvent) || !RequestEventStart(ScheduledEvent))
	{
		UE_LOG(LogNPMapEventManager, Warning,
			TEXT("자동 이벤트 후보가 없거나 시작에 실패하여 스케줄을 중지합니다: Started=%d Target=%d"),
			StartedEventCount, TargetEventCount);
		StopEventScheduling();
	}
	else
	{
		NotifyEventScheduleChanged();
	}
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
