#if WITH_DEV_AUTOMATION_TESTS

#include "Gameplay/Character/Component/NPVisionRestrictionComponent.h"
#include "Misc/AutomationTest.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPVisionFogTransitionTest,
	"NoPhotos.MapEvents.Invisibility.VisionFogTransition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNPVisionFogTransitionTest::RunTest(const FString& Parameters)
{
	using FFog = UNPVisionRestrictionComponent;
	TestEqual(TEXT("Entering does not jump on tag notification"),
		FFog::AdvanceFogStrength(0.0f, true, 0.0f, 1.0f, 2.0f), 0.0f);
	float Strength = FFog::AdvanceFogStrength(0.0f, true, 0.25f, 1.0f, 2.0f);
	TestEqual(TEXT("Entering gradually applies restriction"), Strength, 0.25f);
	TestEqual(TEXT("Leaving does not jump on tag notification"),
		FFog::AdvanceFogStrength(Strength, false, 0.0f, 1.0f, 2.0f), Strength);
	Strength = FFog::AdvanceFogStrength(Strength, false, 0.25f, 1.0f, 2.0f);
	TestEqual(TEXT("Leaving uses separate restoration duration"), Strength, 0.125f);
	Strength = FFog::AdvanceFogStrength(Strength, true, 0.25f, 1.0f, 2.0f);
	TestEqual(TEXT("Re-entering continues from partially restored strength"), Strength, 0.375f);
	TestEqual(TEXT("Large entry frame clamps at full restriction"),
		FFog::AdvanceFogStrength(Strength, true, 10.0f, 1.0f, 2.0f), 1.0f);
	TestEqual(TEXT("Large exit frame reaches exactly zero for cleanup"),
		FFog::AdvanceFogStrength(Strength, false, 10.0f, 1.0f, 2.0f), 0.0f);
	float SplitFrames = 0.0f;
	for (int32 Frame = 0; Frame < 30; ++Frame)
	{
		SplitFrames = FFog::AdvanceFogStrength(SplitFrames, true, 1.0f / 60.0f, 1.0f, 2.0f);
	}
	TestTrue(TEXT("Transition speed is independent of frame subdivision"),
		FMath::IsNearlyEqual(SplitFrames, FFog::AdvanceFogStrength(0.0f, true, 0.5f, 1.0f, 2.0f), 0.0001f));
	TestEqual(TEXT("Zero fade-in duration applies immediately"),
		FFog::AdvanceFogStrength(0.2f, true, 0.0f, 0.0f, 1.0f), 1.0f);
	TestEqual(TEXT("Zero fade-out duration restores immediately"),
		FFog::AdvanceFogStrength(0.8f, false, 0.0f, 1.0f, 0.0f), 0.0f);
	TestEqual(TEXT("Repeated inactive refresh cannot bring fog back"),
		FFog::AdvanceFogStrength(0.0f, false, 0.25f, 1.0f, 1.0f), 0.0f);
	TestEqual(TEXT("Negative frame time does not reverse progress"),
		FFog::AdvanceFogStrength(0.5f, true, -1.0f, 1.0f, 1.0f), 0.5f);
	TestEqual(TEXT("Invalid duration cannot leave an endless transition"),
		FFog::AdvanceFogStrength(0.5f, false, 0.25f, 1.0f, std::numeric_limits<float>::quiet_NaN()), 0.0f);
	TestEqual(TEXT("Invalid strength is sanitized"),
		FFog::AdvanceFogStrength(std::numeric_limits<float>::quiet_NaN(), true, 0.25f, 1.0f, 1.0f), 0.25f);
	return true;
}

#endif
