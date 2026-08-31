#if WITH_DEV_AUTOMATION_TESTS

#include "Gameplay/MapEvents/Santa/NPSantaFlightTypes.h"
#include "Gameplay/MapEvents/Santa/NPSantaGiftTypes.h"
#include "Misc/AutomationTest.h"

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
	return true;
}

#endif
