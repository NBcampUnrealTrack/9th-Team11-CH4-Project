#if WITH_DEV_AUTOMATION_TESTS

#include "Gameplay/MapEvents/Possession/NPGhostFollowerActor.h"
#include "Gameplay/Character/Component/NPControlReversalComponent.h"
#include "Misc/AutomationTest.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPGhostFadeOpacityTest,
	"NoPhotos.MapEvents.Possession.GhostFadeOpacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNPGhostFadeOpacityTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Fade-in starts invisible"),
		ANPGhostFollowerActor::CalculateFadeOpacity(0.0f, 0.35f, 0.0f, 0.5f), 0.0f);
	TestTrue(TEXT("Fade-in midpoint is half maximum opacity"),
		FMath::IsNearlyEqual(ANPGhostFollowerActor::CalculateFadeOpacity(0.0f, 0.35f, 0.25f, 0.5f), 0.175f));
	TestEqual(TEXT("Fade-in finishes at configured opacity even after a long frame"),
		ANPGhostFollowerActor::CalculateFadeOpacity(0.0f, 0.35f, 2.0f, 0.5f), 0.35f);
	const float InterruptedOpacity = ANPGhostFollowerActor::CalculateFadeOpacity(0.0f, 0.35f, 0.1f, 0.5f);
	TestEqual(TEXT("Interrupted appearance fades out from current opacity without a jump"),
		ANPGhostFollowerActor::CalculateFadeOpacity(InterruptedOpacity, 0.0f, 0.0f, 0.5f), InterruptedOpacity);
	float PreviousOpacity = InterruptedOpacity;
	for (int32 Step = 1; Step <= 10; ++Step)
	{
		const float Opacity = ANPGhostFollowerActor::CalculateFadeOpacity(InterruptedOpacity, 0.0f, Step * 0.05f, 0.5f);
		TestTrue(TEXT("Fade-out is monotonic and stays non-negative"), Opacity <= PreviousOpacity && Opacity >= 0.0f);
		PreviousOpacity = Opacity;
	}
	TestEqual(TEXT("Fade-out finishes invisible"), PreviousOpacity, 0.0f);
	TestEqual(TEXT("Zero fade-in duration displays immediately"),
		ANPGhostFollowerActor::CalculateFadeOpacity(0.0f, 0.35f, 0.0f, 0.0f), 0.35f);
	TestEqual(TEXT("Invalid fade-out duration finishes immediately"),
		ANPGhostFollowerActor::CalculateFadeOpacity(0.35f, 0.0f, 0.0f, std::numeric_limits<float>::quiet_NaN()), 0.0f);
	TestEqual(TEXT("Negative duration finishes immediately"),
		ANPGhostFollowerActor::CalculateFadeOpacity(0.35f, 0.0f, 0.0f, -1.0f), 0.0f);
	TestEqual(TEXT("Opacity is clamped to supported range"),
		ANPGhostFollowerActor::CalculateFadeOpacity(-1.0f, 2.0f, 1.0f, 0.5f), 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPControlReversalInputTest,
	"NoPhotos.MapEvents.Possession.HorizontalInputReversal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNPControlReversalInputTest::RunTest(const FString& Parameters)
{
	// 여러 카메라 방향에서 W/S와 A/D를 모두 반전해야 합니다.
	for (const float Yaw : {0.0f, 90.0f, -90.0f, 37.0f, 180.0f, 397.0f})
	{
		const FRotator View(0.0, Yaw, 0.0);
		const FVector Forward = View.RotateVector(FVector::ForwardVector);
		const FVector Right = View.RotateVector(FVector::RightVector);
		const FString Label = FString::Printf(TEXT("Yaw %.0f: "), Yaw);
		TestTrue(Label + TEXT("D becomes A"),
			UNPControlReversalComponent::ResolveMovementInput(Right, Yaw, true).Equals(-Right, 0.0001));
		TestTrue(Label + TEXT("A becomes D"),
			UNPControlReversalComponent::ResolveMovementInput(-Right, Yaw, true).Equals(Right, 0.0001));
		TestTrue(Label + TEXT("W becomes S"),
			UNPControlReversalComponent::ResolveMovementInput(Forward, Yaw, true).Equals(-Forward, 0.0001));
		TestTrue(Label + TEXT("S becomes W"),
			UNPControlReversalComponent::ResolveMovementInput(-Forward, Yaw, true).Equals(Forward, 0.0001));
		const FVector AnalogInput = Forward * 0.3 + Right * 0.4;
		const FVector Mirrored = UNPControlReversalComponent::ResolveMovementInput(AnalogInput, Yaw, true);
		TestTrue(Label + TEXT("Analog diagonal reverses both axes and preserves magnitude"),
			Mirrored.Equals(-AnalogInput, 0.0001)
			&& FMath::IsNearlyEqual(Mirrored.Size(), AnalogInput.Size(), 0.0001));
		TestTrue(Label + TEXT("Ending effect restores original held input"),
			UNPControlReversalComponent::ResolveMovementInput(AnalogInput, Yaw, false).Equals(AnalogInput, 0.0001));
		TestTrue(Label + TEXT("Full diagonal stays clamped to unit length"),
			UNPControlReversalComponent::ResolveMovementInput(Forward + Right, Yaw, true)
			.Equals((-Forward - Right).GetSafeNormal(), 0.0001));
	}
	TestTrue(TEXT("Stopped input stays zero when effect starts or ends"),
		UNPControlReversalComponent::ResolveMovementInput(FVector::ZeroVector, 37.0f, true).IsZero()
		&& UNPControlReversalComponent::ResolveMovementInput(FVector::ZeroVector, 37.0f, false).IsZero());
	TestTrue(TEXT("Vertical component is not reversed"),
		UNPControlReversalComponent::ResolveMovementInput(FVector(0.2, 0.3, 0.4), 0.0f, true)
		.Equals(FVector(-0.2, -0.3, 0.4), 0.0001));
	TestTrue(TEXT("Non-finite view yaw fails closed"),
		UNPControlReversalComponent::ResolveMovementInput(FVector::RightVector,
			std::numeric_limits<float>::infinity(), true).IsZero());
	// 생성자의 NaN 진단 대신 입력 검증 경로 자체를 검사합니다.
	FVector InvalidInput = FVector::ZeroVector;
	InvalidInput.X = std::numeric_limits<double>::quiet_NaN();
	TestTrue(TEXT("Non-finite input fails closed"),
		UNPControlReversalComponent::ResolveMovementInput(InvalidInput, 0.0f, true).IsZero());
	return true;
}

#endif
