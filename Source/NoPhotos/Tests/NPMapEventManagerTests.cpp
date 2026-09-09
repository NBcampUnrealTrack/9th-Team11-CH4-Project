#if WITH_DEV_AUTOMATION_TESTS

#include "Gameplay/MapEvents/NPMapEventManager.h"
#include "Gameplay/MapEvents/NPMapEventDefinition.h"
#include "Gameplay/MapEvents/Blackout/NPBlackoutMapEvent.h"
#include "Core/Main/NPMainGameState.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPMapEventSchedulingTest,
	"NoPhotos.MapEvents.Manager.CountAndDelay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNPMapEventSchedulingTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper TestWorld;
	if (!TestTrue(TEXT("Create isolated world"), TestWorld.CreateTestWorld(EWorldType::Game))) { return false; }
	UWorld* World = TestWorld.GetTestWorld();
	World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
	if (!TestTrue(TEXT("Begin play"), TestWorld.BeginPlayInTestWorld())) { return false; }
	const auto Advance = [&TestWorld](int32 Frames)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame) { TestWorld.TickTestWorld(0.05f); }
	};
	const auto MakeManager = [](AActor* Owner, int32 Count)
	{
		UNPMapEventManagerComponent* Manager = NewObject<UNPMapEventManagerComponent>(Owner);
		Manager->bStartAutomatically = false;
		Manager->MinimumEventCount = Manager->MaximumEventCount = Count;
		Owner->AddInstanceComponent(Manager);
		Manager->RegisterComponent();
		return Manager;
	};
	const auto MakeEvent = [World](UNPMapEventManagerComponent* Manager, FName Id, float Duration, float Delay)
	{
		ANPBlackoutMapEvent* Event = World->SpawnActor<ANPBlackoutMapEvent>();
		if (!Event) { return static_cast<ANPMapEvent*>(nullptr); }
		UNPMapEventDefinition* Definition = NewObject<UNPMapEventDefinition>(Event);
		FindFProperty<FNameProperty>(Definition->GetClass(), TEXT("EventId"))->SetPropertyValue_InContainer(Definition, Id);
		FindFProperty<FFloatProperty>(Definition->GetClass(), TEXT("Duration"))->SetPropertyValue_InContainer(Definition, Duration);
		FindFProperty<FFloatProperty>(Definition->GetClass(), TEXT("Delay"))->SetPropertyValue_InContainer(Definition, Delay);
		Event->InitializeEvent(Definition);
		if (Manager) { Manager->RegisterManagedEvent(Event); }
		return static_cast<ANPMapEvent*>(Event);
	};

	AActor* Owner = World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("Owner"), Owner)) { return false; }
	UNPMapEventManagerComponent* Manager = MakeManager(Owner, 3);
	MakeEvent(Manager, TEXT("A"), 0.0f, 0.2f);
	MakeEvent(Manager, TEXT("B"), 0.0f, 0.2f);
	MakeEvent(Manager, TEXT("C"), 0.0f, 0.2f);
	MakeEvent(Manager, TEXT("A"), 0.0f, 0.2f); // Duplicate catalog identity, distinct actor and definition.
	Manager->FirstEventStartTimeSeconds = 0.4f;
	Advance(2);
	Manager->StartEventScheduling();
	const auto InitialPlan = Manager->GetEventSchedule();
	if (!TestEqual(TEXT("Three distinct events planned upfront"), InitialPlan.Events.Num(), 3)) { return false; }
	TSet<FName> PlannedIds;
	for (const auto& Entry : InitialPlan.Events) { PlannedIds.Add(Entry.EventId); }
	TestEqual(TEXT("Duplicate catalog IDs cannot repeat in plan"), PlannedIds.Num(), 3);
	TestTrue(TEXT("Fixed first time is relative to BeginPlay"), FMath::IsNearlyEqual(
		InitialPlan.Events[0].ExpectedStartServerWorldTime, Manager->ScheduleOriginServerWorldTime + 0.4f, KINDA_SMALL_NUMBER));
	TestEqual(TEXT("Indefinite duration makes later forecast unknown"), InitialPlan.Events[1].ExpectedStartServerWorldTime, -1.0f);
	Advance(2);
	TestEqual(TEXT("No event before fixed start"), Manager->StartedEventCount, 0);
	Manager->StartEventScheduling();
	TestEqual(TEXT("Repeated start does not reroll"), Manager->GetEventSchedule().Events[0].EventId, InitialPlan.Events[0].EventId);
	Advance(6);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		ANPMapEvent* Active = Manager->ScheduledEvent;
		if (!TestNotNull(TEXT("Preselected event starts"), Active)) { return false; }
		TestEqual(TEXT("Execution follows published order"), Active->GetEventId(), InitialPlan.Events[Index].EventId);
		TestEqual(TEXT("Count increases exactly once per start"), Manager->StartedEventCount, Index + 1);
		Active->FinishEvent();
		TestEqual(TEXT("Finished state retained"), Manager->GetEventSchedule().Events[Index].State, ENPScheduledMapEventState::Completed);
		if (Index < 2)
		{
			TestTrue(TEXT("Actual finish resolves next forecast"), Manager->GetNextEventStartRemainingSeconds() > 0.0f);
			Advance(2);
			TestNull(TEXT("Delay prevents immediate next event"), Manager->ScheduledEvent.Get());
			Manager->bLocationLevelTransitionInProgress = true;
			Advance(6);
			TestEqual(TEXT("Unload wait does not consume another slot"), Manager->StartedEventCount, Index + 1);
			Manager->bLocationLevelTransitionInProgress = false;
			Advance(4);
		}
	}
	Advance(100);
	TestFalse(TEXT("Schedule stops after three"), Manager->bScheduling);
	TestEqual(TEXT("Extra time cannot create fourth or fifth event"), Manager->StartedEventCount, 3);
	TestFalse(TEXT("Manual request cannot repeat played types"), Manager->TriggerRandomEvent(ENPMapEventType::TypeA));
	Manager->StartEventScheduling();
	TestEqual(TEXT("Restart cannot reuse already played types in the same match"), Manager->GetEventSchedule().Events.Num(), 0);

	UNPMapEventManagerComponent* ShortPlan = MakeManager(Owner, 3);
	MakeEvent(ShortPlan, TEXT("Only"), 0.0f, 0.0f);
	MakeEvent(ShortPlan, TEXT("Only"), 0.0f, 0.0f);
	ShortPlan->StartEventScheduling();
	TestEqual(TEXT("Insufficient unique types reduce count rather than repeat"), ShortPlan->TargetEventCount, 1);
	ShortPlan->StopEventScheduling();
	Advance(3);
	TestEqual(TEXT("Stop before first start cancels timer"), ShortPlan->StartedEventCount, 0);
	TestEqual(TEXT("Unstarted slot is cancelled"), ShortPlan->GetEventSchedule().Events[0].State, ENPScheduledMapEventState::Cancelled);
	ShortPlan->MinimumEventCount = -1;
	ShortPlan->MaximumEventCount = 0;
	ShortPlan->StartEventScheduling();
	TestTrue(TEXT("Nonpositive counts produce no plan"), ShortPlan->GetEventSchedule().Events.IsEmpty());
	TestFalse(TEXT("Zero count disables scheduling"), ShortPlan->bScheduling);

	UNPMapEventManagerComponent* Bounds = MakeManager(Owner, 3);
	MakeEvent(Bounds, TEXT("BoundsA"), 0.0f, 0.0f);
	MakeEvent(Bounds, TEXT("BoundsB"), 0.0f, 0.0f);
	MakeEvent(Bounds, TEXT("BoundsC"), 0.0f, 0.0f);
	Bounds->MaximumEventCount = 2; // Reversed 3..2 range.
	for (int32 Sample = 0; Sample < 16; ++Sample)
	{
		Bounds->StartEventScheduling();
		TestTrue(TEXT("Reversed bounds normalize to 2..3"), Bounds->TargetEventCount >= 2 && Bounds->TargetEventCount <= 3);
		Bounds->StopEventScheduling();
	}

	UNPMapEventManagerComponent* Timed = MakeManager(Owner, 2);
	MakeEvent(Timed, TEXT("TimedA"), 1.0f, 0.2f);
	MakeEvent(Timed, TEXT("TimedB"), 1.0f, 0.2f);
	Timed->StartEventScheduling();
	const auto TimedPlan = Timed->GetEventSchedule();
	if (!TestEqual(TEXT("Timed plan size"), TimedPlan.Events.Num(), 2)) { return false; }
	TestTrue(TEXT("Forecast includes duration plus delay"), FMath::IsNearlyEqual(
		TimedPlan.Events[1].ExpectedStartServerWorldTime - TimedPlan.Events[0].ExpectedStartServerWorldTime, 1.2f, KINDA_SMALL_NUMBER));
	Advance(2);
	if (!TestNotNull(TEXT("Timed event started"), Timed->ScheduledEvent.Get())) { return false; }
	MakeEvent(Timed, TEXT("LateCandidate"), 1.0f, 0.2f);
	const float EarlyFinishTime = Timed->GetServerWorldTimeSeconds();
	Timed->ScheduledEvent->FinishEvent();
	TestTrue(TEXT("Early completion shifts next expected start"), FMath::IsNearlyEqual(
		Timed->GetEventSchedule().Events[1].ExpectedStartServerWorldTime, EarlyFinishTime + 0.2f, KINDA_SMALL_NUMBER));
	Advance(6);
	if (!TestNotNull(TEXT("Second timed event started"), Timed->ScheduledEvent.Get())) { return false; }
	TestEqual(TEXT("Added candidate cannot replace published event"), Timed->ScheduledEvent->GetEventId(), TimedPlan.Events[1].EventId);
	Timed->StopEventScheduling();
	ANPMapEvent* StoppedActive = Timed->ScheduledEvent;
	TestTrue(TEXT("Ordinary stop preserves running event"), StoppedActive->IsEventActive());
	StoppedActive->FinishEvent();
	TestEqual(TEXT("Stopped active event still reports completion"), Timed->GetEventSchedule().Events[1].State, ENPScheduledMapEventState::Completed);

	// Exercise the real game-end entry point, including concurrent/manual and unmanaged events.
	ANPMainGameState* MainState = World->SpawnActor<ANPMainGameState>();
	if (!TestNotNull(TEXT("Main game state"), MainState)) { return false; }
	World->SetGameState(MainState);
	UNPMapEventManagerComponent* Ending = MakeManager(MainState, 3);
	MakeEvent(Ending, TEXT("EndA"), 0.0f, 0.0f);
	MakeEvent(Ending, TEXT("EndB"), 0.0f, 0.0f);
	MakeEvent(Ending, TEXT("EndC"), 0.0f, 0.0f);
	Ending->StartEventScheduling();
	Advance(2);
	ANPMapEvent* EndingActive = Ending->ScheduledEvent;
	if (!TestNotNull(TEXT("Event active before game end"), EndingActive)) { return false; }
	ANPMapEvent* Manual = MakeEvent(Ending, TEXT("Manual"), 0.0f, 0.0f);
	Ending->bAllowConcurrentEvents = true;
	TestTrue(TEXT("Unreserved manual event can run concurrently"), Ending->TriggerRandomEvent(ENPMapEventType::TypeA));
	TestTrue(TEXT("Manual event is active"), Manual && Manual->IsEventActive());
	ANPMapEvent* Unmanaged = MakeEvent(nullptr, TEXT("Unmanaged"), 0.0f, 0.0f);
	if (!TestNotNull(TEXT("Unmanaged event"), Unmanaged)) { return false; }
	TestTrue(TEXT("Unmanaged event starts"), Unmanaged->StartEvent());
	UNPMapEventManagerComponent* Loading = MakeManager(MainState, 1);
	ANPMapEvent* Pending = MakeEvent(Loading, TEXT("Pending"), 0.0f, 0.0f);
	Loading->FirstEventStartTimeSeconds = 100.0f;
	Loading->StartEventScheduling();
	Loading->PendingEvent = Pending;
	Loading->ScheduledEvent = Pending;
	Loading->bLocationLevelTransitionInProgress = true;
	MainState->FinishMainGame();
	TestFalse(TEXT("Game end finishes scheduled event"), EndingActive->IsEventActive());
	TestFalse(TEXT("Game end finishes manual event"), Manual->IsEventActive());
	TestFalse(TEXT("Game end finishes unmanaged event"), Unmanaged->IsEventActive());
	TestTrue(TEXT("Manager is permanently blocked after game end"), Ending->bEventsShutdown);
	TestEqual(TEXT("Forced running slot is cancelled"), Ending->GetEventSchedule().Events[0].State, ENPScheduledMapEventState::Cancelled);
	TestEqual(TEXT("Future slot is cancelled"), Ending->GetEventSchedule().Events[1].State, ENPScheduledMapEventState::Cancelled);
	TestTrue(TEXT("Active UI list cleared"), Ending->GetActiveEventPresentations().IsEmpty());
	TestNull(TEXT("Pending load cancelled"), Loading->PendingEvent.Get());
	Loading->HandleLocationLevelShown();
	Loading->HandleNextEventTimer();
	Ending->StartEventScheduling();
	TestFalse(TEXT("Schedule cannot restart after match end"), Ending->bScheduling);
	TestFalse(TEXT("Manual trigger rejected after match end"), Ending->TriggerRandomEvent(ENPMapEventType::TypeA));
	TestFalse(TEXT("Direct actor start rejected after match end"), Unmanaged->StartEvent());
	Advance(10);
	TestFalse(TEXT("Late loading/timer callbacks cannot start cancelled event"), Pending->IsEventActive());
	MainState->FinishMainGame();
	Ending->ShutdownEventsForGameEnd();
	TestTrue(TEXT("Repeated shutdown is harmless"), Ending->GetActiveEventPresentations().IsEmpty());
	return true;
}

#endif
