#include "NPLegacyMapEventManager.h"

#include "Gameplay/MapEvents/ArtifactSpawn/NPArtifactSpawnMapEvent.h"
#include "Gameplay/MapEvents/Blackout/NPBlackoutMapEvent.h"
#include "Gameplay/MapEvents/NPMapEvent.h"
#include "Gameplay/MapEvents/SpeedBoost/NPSpeedBoostMapEvent.h"
#include "Engine/World.h"
#include "TimerManager.h"

ANPLegacyMapEventManager::ANPLegacyMapEventManager()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);

	// Legacy design: adding an event requires a concrete include and manager edit.
	EventClasses.Add(ANPBlackoutMapEvent::StaticClass());
	EventClasses.Add(ANPArtifactSpawnMapEvent::StaticClass());
	EventClasses.Add(ANPSpeedBoostMapEvent::StaticClass());
}

void ANPLegacyMapEventManager::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	CreateEventInstances();
	if (bStartAutomatically)
	{
		StartEventScheduling();
	}
}

void ANPLegacyMapEventManager::StartEventScheduling()
{
	if (!HasAuthority() || EventInstances.IsEmpty())
	{
		return;
	}

	ScheduleNextEvent(InitialDelay);
}

void ANPLegacyMapEventManager::StopEventScheduling()
{
	if (HasAuthority())
	{
		GetWorldTimerManager().ClearTimer(EventTimer);
	}
}

bool ANPLegacyMapEventManager::TriggerRandomEvent()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World || (!bAllowConcurrentEvents && HasActiveEvent()))
	{
		return false;
	}

	TArray<ANPMapEvent*> Candidates;

	for (ANPMapEvent* EventInstance : EventInstances)
	{
		if (IsValid(EventInstance) && EventInstance->CanStartEvent())
		{
			Candidates.Add(EventInstance);
		}
	}

	if (Candidates.IsEmpty())
	{
		return false;
	}

	return Candidates[FMath::RandRange(0, Candidates.Num() - 1)]->StartEvent();
}

void ANPLegacyMapEventManager::CreateEventInstances()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World)
	{
		return;
	}

	for (const TSubclassOf<ANPMapEvent>& EventClass : EventClasses)
	{
		if (!EventClass)
		{
			continue;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = this;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (ANPMapEvent* EventInstance = World->SpawnActor<ANPMapEvent>(
			EventClass, GetActorTransform(), SpawnParameters))
		{
			EventInstances.Add(EventInstance);
		}
	}
}

void ANPLegacyMapEventManager::ScheduleNextEvent(const float Delay)
{
	GetWorldTimerManager().SetTimer(
		EventTimer,
		this,
		&ANPLegacyMapEventManager::HandleEventTimer,
		FMath::Max(Delay, 0.01f),
		false);
}

void ANPLegacyMapEventManager::HandleEventTimer()
{
	TriggerRandomEvent();

	const float LowerBound = FMath::Min(MinimumInterval, MaximumInterval);
	const float UpperBound = FMath::Max(MinimumInterval, MaximumInterval);
	ScheduleNextEvent(FMath::FRandRange(LowerBound, UpperBound));
}

bool ANPLegacyMapEventManager::HasActiveEvent() const
{
	return EventInstances.ContainsByPredicate(
		[](const ANPMapEvent* EventInstance)
		{
			return IsValid(EventInstance) && EventInstance->IsEventActive();
		});
}
