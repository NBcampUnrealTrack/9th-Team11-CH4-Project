#if WITH_DEV_AUTOMATION_TESTS

#include "Gameplay/MapEvents/Santa/NPSantaFlightTypes.h"
#include "Gameplay/MapEvents/Santa/NPSantaGiftTypes.h"
#include "Gameplay/MapEvents/Santa/NPSantaEventDefinition.h"
#include "Gameplay/MapEvents/Santa/NPSantaFlightActor.h"
#include "Gameplay/MapEvents/Santa/NPSantaFlightRoute.h"
#include "Gameplay/MapEvents/Santa/NPSantaGiftActor.h"
#include "Gameplay/MapEvents/Santa/NPSantaMapEvent.h"
#include "Gameplay/Relic/NPPulleyPictureRelic.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "UObject/UnrealType.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPSantaStraightPathTest,
	"NoPhotos.MapEvents.Santa.StraightPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNPSantaStraightPathTest::RunTest(const FString& Parameters)
{
	const FVector Origin(1000.0, -500.0, 200.0);
	FVector Start, End;
	TestTrue(TEXT("Translated route is valid"), NPSantaFlight::BuildStraightPath(
		FTransform(FRotator::ZeroRotator, Origin), 2000.0f, 12000.0f, Start, End));
	TestTrue(TEXT("Start uses center minus half distance"), Start.Equals(FVector(-5000.0, -500.0, 2200.0)));
	TestTrue(TEXT("End uses center plus half distance"), End.Equals(FVector(7000.0, -500.0, 2200.0)));

	TestTrue(TEXT("Yaw 90 route is valid"), NPSantaFlight::BuildStraightPath(
		FTransform(FRotator(0.0, 90.0, 0.0), Origin), 2000.0f, 12000.0f, Start, End));
	TestTrue(TEXT("Yaw 90 flies along world Y"), (End - Start).Equals(FVector(0.0, 12000.0, 0.0), 0.01));
	TestTrue(TEXT("Changing heading preserves route center"), ((Start + End) * 0.5).Equals(Origin + FVector(0.0, 0.0, 2000.0)));

	FVector ReferenceStart, ReferenceEnd;
	NPSantaFlight::BuildStraightPath(FTransform(FRotator(0.0, 37.0, 0.0), Origin),
		2000.0f, 12000.0f, ReferenceStart, ReferenceEnd);
	TestTrue(TEXT("Tilted and scaled route is valid"), NPSantaFlight::BuildStraightPath(
		FTransform(FRotator(20.0, 37.0, 15.0), Origin, FVector(2.0, 3.0, 4.0)),
		2000.0f, 12000.0f, Start, End));
	TestTrue(TEXT("Pitch, roll and scale do not change path"),
		Start.Equals(ReferenceStart, 0.01) && End.Equals(ReferenceEnd, 0.01));
	TestFalse(TEXT("Zero distance rejected"), NPSantaFlight::BuildStraightPath(FTransform::Identity, 2000.0f, 0.0f, Start, End));
	TestFalse(TEXT("Negative distance rejected"), NPSantaFlight::BuildStraightPath(FTransform::Identity, 2000.0f, -1.0f, Start, End));
	TestFalse(TEXT("Negative height rejected"), NPSantaFlight::BuildStraightPath(FTransform::Identity, -1.0f, 12000.0f, Start, End));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPSantaFlightPlanTest,
	"NoPhotos.MapEvents.Santa.FlightPlan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNPSantaFlightPlanTest::RunTest(const FString& Parameters)
{
	FNPSantaFlightPlan Plan;
	TestFalse(TEXT("Uninitialized plan is invalid"), Plan.IsValid());
	Plan.StartLocation = FVector(1000.0, -6000.0, 2200.0);
	Plan.EndLocation = FVector(1000.0, 6000.0, 2200.0);
	Plan.StartServerTime = 100.0f;
	Plan.Duration = 30.0f;
	TestTrue(TEXT("Configured plan is valid"), Plan.IsValid());
	TestEqual(TEXT("Before start clamps to zero"), Plan.GetProgress(90.0f), 0.0f);
	TestEqual(TEXT("Mid-flight join uses elapsed server time"), Plan.GetProgress(115.0f), 0.5f);
	TestEqual(TEXT("After finish clamps to one"), Plan.GetProgress(150.0f), 1.0f);
	TestTrue(TEXT("Midpoint follows the selected Y route"),
		Plan.GetTransform(Plan.GetProgress(115.0f)).GetLocation().Equals(FVector(1000.0, 0.0, 2200.0)));
	TestTrue(TEXT("Forward vector follows the route"),
		Plan.GetTransform(0.5f).GetRotation().GetForwardVector().Equals(FVector::YAxisVector, 0.001));
	TestTrue(TEXT("Transform progress clamps to end"), Plan.GetTransform(2.0f).GetLocation().Equals(Plan.EndLocation));
	const FVector RouteStart = Plan.StartLocation;
	const FVector RouteEnd = Plan.EndLocation;
	Plan.SetRouteEndpoints(RouteStart, RouteEnd, true);
	TestTrue(TEXT("Reverse starts at the original route end"), Plan.GetTransform(0.0f).GetLocation().Equals(RouteEnd));
	TestTrue(TEXT("Reverse finishes at the original route start"), Plan.GetTransform(1.0f).GetLocation().Equals(RouteStart));
	TestTrue(TEXT("Reverse flight faces its travel direction"),
		Plan.GetTransform(0.5f).GetRotation().GetForwardVector().Equals(-FVector::YAxisVector, 0.001));
	TestEqual(TEXT("Reverse preserves server-time progress"), Plan.GetProgress(115.0f), 0.5f);
	Plan.SetRouteEndpoints(RouteStart, RouteEnd, false);
	TestTrue(TEXT("Forward selection preserves endpoints"), Plan.StartLocation.Equals(RouteStart) && Plan.EndLocation.Equals(RouteEnd));
	Plan.Duration = 0.0f;
	TestFalse(TEXT("Zero duration rejected"), Plan.IsValid());
	TestEqual(TEXT("Invalid duration does not divide by zero"), Plan.GetProgress(115.0f), 0.0f);
	Plan.Duration = 30.0f;
	Plan.EndLocation = Plan.StartLocation;
	TestFalse(TEXT("Coincident endpoints rejected"), Plan.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPSantaGiftScheduleTest,
	"NoPhotos.MapEvents.Santa.GiftSchedule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNPSantaGiftScheduleTest::RunTest(const FString& Parameters)
{
	FNPSantaGiftDropSchedule Schedule;
	Schedule.Count = 3;
	Schedule.StartProgress = 0.2f;
	Schedule.EndProgress = 0.8f;
	float Progress = 0.0f;
	TestTrue(TEXT("First gift has a schedule"), Schedule.GetDropProgress(0, Progress));
	TestEqual(TEXT("First gift starts at configured progress"), Progress, 0.2f);
	TestTrue(TEXT("Middle gift has a schedule"), Schedule.GetDropProgress(1, Progress));
	TestTrue(TEXT("Gifts are evenly distributed"), FMath::IsNearlyEqual(Progress, 0.5f));
	TestTrue(TEXT("Last gift has a schedule"), Schedule.GetDropProgress(2, Progress));
	TestEqual(TEXT("Last gift ends at configured progress"), Progress, 0.8f);
	TestFalse(TEXT("No extra gift after requested count"), Schedule.GetDropProgress(3, Progress));
	TestFalse(TEXT("Negative slot rejected"), Schedule.GetDropProgress(-1, Progress));
	Schedule.RandomDropRadius = 500.0f;
	TestTrue(TEXT("Random offset stays horizontal"), FMath::IsNearlyZero(Schedule.GetRandomDropOffset(0.25f, 0.5f).Z));
	TestTrue(TEXT("Random offset stays inside configured radius"), Schedule.GetRandomDropOffset(0.25f, 1.0f).Size2D() <= 500.0f);
	TestTrue(TEXT("Area sample reaches configured boundary"), FMath::IsNearlyEqual(Schedule.GetRandomDropOffset(0.0f, 1.0f).Size2D(), 500.0f));
	Schedule.RandomDropRadius = 0.0f;
	TestTrue(TEXT("Zero radius preserves the original straight drop"), Schedule.GetRandomDropOffset(0.3f, 0.8f).IsNearlyZero());
	Schedule.RandomDropRadius = 500.0f;
	Schedule.Count = 1;
	TestTrue(TEXT("Single gift does not divide by zero"), Schedule.GetDropProgress(0, Progress));
	TestEqual(TEXT("Single gift uses start progress"), Progress, 0.2f);
	Schedule.Count = 0;
	TestTrue(TEXT("Flight-only configuration is valid"), Schedule.IsValid());
	TestFalse(TEXT("Flight-only configuration has no gift"), Schedule.GetDropProgress(0, Progress));
	Schedule.Count = 3;
	Schedule.EndProgress = Schedule.StartProgress;
	TestFalse(TEXT("Multiple gifts cannot share one schedule time"), Schedule.IsValid());
	Schedule.EndProgress = 0.1f;
	TestFalse(TEXT("Reversed interval rejected"), Schedule.IsValid());
	Schedule.EndProgress = 1.0f;
	TestFalse(TEXT("Drop at flight finish rejected to avoid duration timer race"), Schedule.IsValid());
	Schedule.EndProgress = 0.8f;
	Schedule.Count = 129;
	TestFalse(TEXT("Runtime count limit enforced"), Schedule.IsValid());
	Schedule.Count = 3;
	Schedule.RandomDropRadius = -1.0f;
	TestFalse(TEXT("Negative random drop radius rejected"), Schedule.IsValid());
	Schedule.RandomDropRadius = std::numeric_limits<float>::infinity();
	TestFalse(TEXT("Infinite random drop radius rejected"), Schedule.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPSantaFlightScheduleTest,
	"NoPhotos.MapEvents.Santa.FlightSchedule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNPSantaFlightScheduleTest::RunTest(const FString& Parameters)
{
	FNPSantaFlightSchedule Schedule;
	Schedule.RespawnDelayMin = 2.0f;
	Schedule.RespawnDelayMax = 6.0f;
	TestEqual(TEXT("Lower endpoint is included"), Schedule.GetRespawnDelay(0.0f), 2.0f);
	TestEqual(TEXT("Upper endpoint is included"), Schedule.GetRespawnDelay(1.0f), 6.0f);
	TestEqual(TEXT("Intermediate sample interpolates the interval"), Schedule.GetRespawnDelay(0.25f), 3.0f);
	FRandomStream Random(12345);
	float FirstDelay = Schedule.GetRespawnDelay(Random.FRand());
	bool bSawDifferentDelay = false;
	for (int32 Index = 0; Index < 128; ++Index)
	{
		const float Delay = Schedule.GetRespawnDelay(Random.FRand());
		TestTrue(TEXT("Every delay stays in configured bounds"), Delay >= 2.0f && Delay <= 6.0f);
		bSawDifferentDelay |= !FMath::IsNearlyEqual(Delay, FirstDelay);
	}
	TestTrue(TEXT("Resampling does not produce a fixed delay"), bSawDifferentDelay);
	Schedule.RespawnDelayMax = Schedule.RespawnDelayMin;
	TestEqual(TEXT("Equal bounds allow a fixed delay"), Schedule.GetRespawnDelay(0.7f), 2.0f);
	Schedule.RespawnDelayMin = Schedule.RespawnDelayMax = 0.0f;
	TestTrue(TEXT("Zero delay is valid for next-tick scheduling"), Schedule.IsValid());
	Schedule.RespawnDelayMin = -1.0f;
	TestFalse(TEXT("Negative delay rejected"), Schedule.IsValid());
	Schedule.RespawnDelayMin = 2.0f;
	Schedule.RespawnDelayMax = 1.0f;
	TestFalse(TEXT("Reversed range rejected"), Schedule.IsValid());
	Schedule.RespawnDelayMax = std::numeric_limits<float>::infinity();
	TestFalse(TEXT("Infinite delay rejected"), Schedule.IsValid());
	Schedule.RespawnDelayMax = 3.0f;
	Schedule.FlightDuration = 0.0f;
	TestFalse(TEXT("Zero flight duration rejected"), Schedule.IsValid());
	Schedule.FlightDuration = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("NaN flight duration rejected"), Schedule.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPSantaRepeatedFlightTest,
	"NoPhotos.MapEvents.Santa.RepeatedFlightLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNPSantaRepeatedFlightTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper TestWorld;
	if (!TestTrue(TEXT("Create isolated test world"), TestWorld.CreateTestWorld(EWorldType::Game)))
	{
		return false;
	}
	UWorld* World = TestWorld.GetTestWorld();
	World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
	if (!TestTrue(TEXT("Begin play without project maps or blueprints"), TestWorld.BeginPlayInTestWorld()))
	{
		return false;
	}
	ANPSantaFlightRoute* RouteA = World->SpawnActor<ANPSantaFlightRoute>();
	ANPSantaFlightRoute* RouteB = World->SpawnActor<ANPSantaFlightRoute>(FVector(0.0, 20000.0, 0.0), FRotator::ZeroRotator);
	ANPSantaMapEvent* Event = World->SpawnActor<ANPSantaMapEvent>();
	if (!TestNotNull(TEXT("First route"), RouteA) || !TestNotNull(TEXT("Second route"), RouteB)
		|| !TestNotNull(TEXT("Event"), Event))
	{
		return false;
	}
	UNPSantaEventDefinition* Definition = NewObject<UNPSantaEventDefinition>(Event);
	// Set the same reflected fields edited in DA_Santa, without changing any project assets.
	FindFProperty<FClassProperty>(Definition->GetClass(), TEXT("SantaClass"))
		->SetObjectPropertyValue_InContainer(Definition, ANPSantaFlightActor::StaticClass());
	FindFProperty<FClassProperty>(Definition->GetClass(), TEXT("GiftClass"))
		->SetObjectPropertyValue_InContainer(Definition, ANPSantaGiftActor::StaticClass());
	FFloatProperty* DurationProperty = FindFProperty<FFloatProperty>(Definition->GetClass(), TEXT("Duration"));
	DurationProperty->SetPropertyValue_InContainer(Definition, 2.0f);
	FNPSantaFlightSchedule& Schedule = *FindFProperty<FStructProperty>(Definition->GetClass(), TEXT("FlightSchedule"))
		->ContainerPtrToValuePtr<FNPSantaFlightSchedule>(Definition);
	Schedule.FlightDuration = 0.2f;
	Schedule.RespawnDelayMin = 0.1f;
	Schedule.RespawnDelayMax = 0.2f;
	FNPSantaGiftDropSchedule& Gifts = *FindFProperty<FStructProperty>(Definition->GetClass(), TEXT("GiftDrops"))
		->ContainerPtrToValuePtr<FNPSantaGiftDropSchedule>(Definition);
	Gifts.Count = 1;
	Gifts.StartProgress = 0.25f;
	FindFProperty<FArrayProperty>(Definition->GetClass(), TEXT("RelicClasses"))
		->ContainerPtrToValuePtr<TArray<TSubclassOf<ANPBaseRelic>>>(Definition)->Add(ANPPulleyPictureRelic::StaticClass());
	Event->InitializeEvent(Definition, 1.0f);
	const auto Advance = [&TestWorld](int32 Frames)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			TestWorld.TickTestWorld(0.01f);
		}
	};
	const auto CountGifts = [World]()
	{
		int32 Count = 0;
		for (TActorIterator<ANPSantaGiftActor> It(World); It; ++It) { ++Count; }
		return Count;
	};
	TestTrue(TEXT("Start event"), Event->StartEvent());
	if (!TestNotNull(TEXT("First flight starts immediately"), Event->GetSanta())) { return false; }
	TWeakObjectPtr<ANPSantaFlightActor> PreviousSanta;
	FNPSantaFlightPlan PreviousPlan;
	int32 FlightCount = 0;
	bool bSawWaiting = false;
	for (int32 Frame = 0; Frame < 220; ++Frame)
	{
		ANPSantaFlightActor* Santa = Event->GetSanta();
		if (Santa && Santa != PreviousSanta.Get())
		{
			const FNPSantaFlightPlan Plan = Santa->GetFlightPlan();
			TestEqual(TEXT("Each pass uses flight duration, not event duration"), Plan.Duration, 0.2f);
			TestTrue(TEXT("New pass starts at current time, not original event start"), Santa->GetFlightProgress() < 0.06f);
			if (FlightCount > 0)
			{
				const float Delay = Plan.StartServerTime - (PreviousPlan.StartServerTime + PreviousPlan.Duration);
				TestTrue(TEXT("Repeated flight waits a sampled interval after prior pass"), Delay >= 0.099f && Delay <= 0.23f);
				TestFalse(TEXT("Multiple routes do not immediately repeat"),
					Plan.GetTransform(0.5f).GetLocation().Equals(PreviousPlan.GetTransform(0.5f).GetLocation()));
			}
			PreviousSanta = Santa;
			PreviousPlan = Plan;
			++FlightCount;
		}
		bSawWaiting |= Event->IsEventActive() && Santa == nullptr;
		int32 SantaCount = 0;
		for (TActorIterator<ANPSantaFlightActor> It(World); It; ++It) { ++SantaCount; }
		TestTrue(TEXT("At most one flight exists at a time"), SantaCount <= 1);
		Advance(1);
	}
	TestTrue(TEXT("Several passes occur during one event"), FlightCount >= 4);
	TestTrue(TEXT("Event stays active between flights"), bSawWaiting);
	TestFalse(TEXT("Total duration finishes the event"), Event->IsEventActive());
	TestNull(TEXT("Expiry removes the last flight"), Event->GetSanta());
	TestTrue(TEXT("Gift schedule restarts on each pass and old gifts survive"), CountGifts() >= FlightCount - 1);
	TestTrue(TEXT("No duplicate drops per pass"), CountGifts() <= FlightCount);
	const int32 GiftsAtExpiry = CountGifts();
	Advance(30);
	TestEqual(TEXT("Expiry cancels future drops"), CountGifts(), GiftsAtExpiry);

	TestTrue(TEXT("Restart event"), Event->StartEvent());
	Advance(23);
	TestNull(TEXT("Waiting after first pass"), Event->GetSanta());
	Event->FinishEvent();
	const int32 GiftsAtCancel = CountGifts();
	Advance(35);
	TestNull(TEXT("Manual finish cancels pending respawn"), Event->GetSanta());
	TestEqual(TEXT("Manual finish preserves gifts and cancels drops"), CountGifts(), GiftsAtCancel);
	TestTrue(TEXT("Restart after cancellation"), Event->StartEvent());
	if (!TestNotNull(TEXT("Restart creates fresh Santa"), Event->GetSanta())) { return false; }
	Event->GetSanta()->Destroy();
	TestTrue(TEXT("External Santa destruction keeps event alive"), Event->IsEventActive());
	Advance(23);
	TestNotNull(TEXT("External destruction schedules another flight"), Event->GetSanta());
	Event->FinishEvent();

	RouteB->Destroy();
	Schedule.RespawnDelayMin = Schedule.RespawnDelayMax = 0.0f;
	TestTrue(TEXT("One remaining route is reusable"), Event->StartEvent());
	Advance(24);
	TestNotNull(TEXT("Zero delay respawns on a later tick"), Event->GetSanta());
	Event->FinishEvent();
	DurationProperty->SetPropertyValue_InContainer(Definition, 0.1f);
	TestTrue(TEXT("Short remaining event can start a partial pass"), Event->StartEvent());
	if (TestNotNull(TEXT("Partial pass Santa"), Event->GetSanta()))
	{
		TestEqual(TEXT("Partial final pass does not speed up"), Event->GetSanta()->GetFlightPlan().Duration, 0.2f);
	}
	Advance(15);
	TestFalse(TEXT("Event expires during partial pass"), Event->IsEventActive());
	TestNull(TEXT("Partial pass cleaned up"), Event->GetSanta());

	DurationProperty->SetPropertyValue_InContainer(Definition, 1.0f);
	Schedule.RespawnDelayMin = Schedule.RespawnDelayMax = 0.2f;
	TestTrue(TEXT("Start event for EndPlay cleanup"), Event->StartEvent());
	Advance(23);
	TestNull(TEXT("Pending respawn before owner removal"), Event->GetSanta());
	Event->Destroy();
	const int32 GiftsAtDestroy = CountGifts();
	Advance(40);
	TestEqual(TEXT("Owner removal cancels future drops"), CountGifts(), GiftsAtDestroy);
	int32 RemainingSantaCount = 0;
	for (TActorIterator<ANPSantaFlightActor> It(World); It; ++It) { ++RemainingSantaCount; }
	TestEqual(TEXT("Owner removal cancels pending respawn"), RemainingSantaCount, 0);
	return true;
}

#endif
