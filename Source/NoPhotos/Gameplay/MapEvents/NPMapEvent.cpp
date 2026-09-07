#include "NPMapEvent.h"

#include "Engine/World.h"
#include "Core/Main/NPMainGameState.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "NPMapEventDefinition.h"
#include "TimerManager.h"

ANPMapEvent::ANPMapEvent()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);
}

void ANPMapEvent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANPMapEvent, EventDefinition);
	DOREPLIFETIME(ANPMapEvent, bIsActive);
	DOREPLIFETIME(ANPMapEvent, EventEndServerWorldTime);
}

void ANPMapEvent::InitializeEvent(
	UNPMapEventDefinition* InEventDefinition,
	const float InSelectionWeight)
{
	if (!HasAuthority() || bIsActive)
	{
		return;
	}

	EventDefinition = InEventDefinition;
	RuntimeSelectionWeight = FMath::Max(0.0f, InSelectionWeight);
}

FName ANPMapEvent::GetEventId() const
{
	return EventDefinition ? EventDefinition->GetEventId() : EventId;
}

FText ANPMapEvent::GetEventDisplayName() const
{
	return EventDefinition ? EventDefinition->GetDisplayName() : EventDisplayName;
}

FText ANPMapEvent::GetEventDescription() const
{
	return EventDefinition ? EventDefinition->GetDescription() : EventDescription;
}

ENPMapEventType ANPMapEvent::GetEventType() const
{
	return EventDefinition ? EventDefinition->GetEventType() : EventType;
}

ENPMapEventScale ANPMapEvent::GetEventScale() const
{
	return EventDefinition ? EventDefinition->GetEventScale() : EventScale;
}

float ANPMapEvent::GetEventDuration() const
{
	return EventDefinition ? EventDefinition->GetDuration() : FMath::Max(0.0f, Duration);
}

float ANPMapEvent::GetRemainingEventTime() const
{
	if (!bIsActive || EventEndServerWorldTime <= 0.0f)
	{
		return 0.0f;
	}

	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	const float CurrentServerWorldTime = GameState
		? GameState->GetServerWorldTimeSeconds()
		: (World ? World->GetTimeSeconds() : 0.0f);
	return FMath::Max(0.0f, EventEndServerWorldTime - CurrentServerWorldTime);
}

bool ANPMapEvent::CanRunAtEventTime(const ENPMapEventType EventTimeType) const
{
	const ENPMapEventType ConfiguredType = GetEventType();
	return ConfiguredType == ENPMapEventType::Both || ConfiguredType == EventTimeType;
}

bool ANPMapEvent::StartEvent()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World || !CanStartEvent())
	{
		return false;
	}

	const float EventDuration = GetEventDuration();
	const AGameStateBase* GameState = World->GetGameState();
	const float CurrentServerWorldTime = GameState
		? GameState->GetServerWorldTimeSeconds()
		: World->GetTimeSeconds();
	EventEndServerWorldTime = EventDuration > 0.0f
		? CurrentServerWorldTime + EventDuration
		: 0.0f;
	bIsActive = true;
	ApplyEventState(true);
	OnEventStarted.Broadcast(this);
	ForceNetUpdate();

	if (EventDuration > 0.0f)
	{
		World->GetTimerManager().SetTimer(DurationTimer, this, &ANPMapEvent::FinishEvent, EventDuration, false);
	}

	return true;
}

void ANPMapEvent::FinishEvent()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World || !bIsActive)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(DurationTimer);
	EventEndServerWorldTime = 0.0f;
	bIsActive = false;
	ApplyEventState(false);
	OnEventFinished.Broadcast(this);
	ForceNetUpdate();
}

bool ANPMapEvent::CanStartEvent() const
{
	const ANPMainGameState* MainState = GetWorld() ? GetWorld()->GetGameState<ANPMainGameState>() : nullptr;
	return HasAuthority()
		&& (!MainState || !MainState->IsMainGameEnded())
		&& !bIsActive
		&& RuntimeSelectionWeight > 0.0f;
}

void ANPMapEvent::ApplyEventState_Implementation(const bool bNewActive)
{
}

void ANPMapEvent::OnRep_IsActive()
{
	ApplyEventState(bIsActive);
	if (bIsActive)
	{
		OnEventStarted.Broadcast(this);
	}
	else
	{
		OnEventFinished.Broadcast(this);
	}
}
