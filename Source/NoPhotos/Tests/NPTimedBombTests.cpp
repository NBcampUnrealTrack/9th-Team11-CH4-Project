#if WITH_DEV_AUTOMATION_TESTS

#include "Gameplay/MapEvents/Bomb/NPTimedBomb.h"
#include "Misc/AutomationTest.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPTimedBombBlastVelocityTest,
	"NoPhotos.MapEvents.Bomb.BlastVelocity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNPTimedBombBlastVelocityTest::RunTest(const FString& Parameters)
{
	const FVector Origin(1000.0, -2000.0, 100.0);
	TestTrue(TEXT("Target to the right is pushed outward and upward"),
		ANPTimedBomb::CalculateBlastVelocity(Origin, Origin + FVector(100.0, 0.0, 0.0), 500.0f, 1000.0f, 600.0f)
		.Equals(FVector(1000.0, 0.0, 600.0)));
	TestTrue(TEXT("Opposite target is pushed away, not in a fixed world direction"),
		ANPTimedBomb::CalculateBlastVelocity(Origin, Origin - FVector(0.0, 200.0, 0.0), 500.0f, 1000.0f, 600.0f)
		.Equals(FVector(0.0, -1000.0, 600.0)));
	TestTrue(TEXT("Exact center has a finite upward-only velocity"),
		ANPTimedBomb::CalculateBlastVelocity(Origin, Origin, 500.0f, 1000.0f, 600.0f)
		.Equals(FVector(0.0, 0.0, 600.0)));
	TestTrue(TEXT("Exact radius is included"),
		!ANPTimedBomb::CalculateBlastVelocity(Origin, Origin + FVector(500.0, 0.0, 0.0), 500.0f, 1000.0f, 600.0f).IsZero());
	TestTrue(TEXT("Outside radius is not affected"),
		ANPTimedBomb::CalculateBlastVelocity(Origin, Origin + FVector(500.1, 0.0, 0.0), 500.0f, 1000.0f, 600.0f).IsZero());
	TestTrue(TEXT("Blast range is a sphere, not an infinite vertical cylinder"),
		ANPTimedBomb::CalculateBlastVelocity(Origin, Origin + FVector(0.0, 0.0, 501.0), 500.0f, 1000.0f, 600.0f).IsZero());
	const FVector DiagonalVelocity = ANPTimedBomb::CalculateBlastVelocity(
		Origin, Origin + FVector(100.0, 100.0, 100.0), 500.0f, 1000.0f, 600.0f);
	TestTrue(TEXT("Diagonal direction preserves horizontal strength and independent upward strength"),
		FMath::IsNearlyEqual(DiagonalVelocity.Size2D(), 1000.0, 0.001) && FMath::IsNearlyEqual(DiagonalVelocity.Z, 600.0));
	TestTrue(TEXT("Zero radius disables knockback"),
		ANPTimedBomb::CalculateBlastVelocity(Origin, Origin, 0.0f, 1000.0f, 600.0f).IsZero());
	TestTrue(TEXT("Negative strengths do not pull targets inward or downward"),
		ANPTimedBomb::CalculateBlastVelocity(Origin, Origin + FVector(100.0, 0.0, 0.0), 500.0f, -1000.0f, -600.0f).IsZero());
	TestTrue(TEXT("Non-finite radius is rejected"),
		ANPTimedBomb::CalculateBlastVelocity(Origin, Origin, std::numeric_limits<float>::infinity(), 1000.0f, 600.0f).IsZero());
	TestTrue(TEXT("Non-finite strength is rejected"),
		ANPTimedBomb::CalculateBlastVelocity(Origin, Origin, 500.0f, std::numeric_limits<float>::quiet_NaN(), 600.0f).IsZero());
	return true;
}

#endif
