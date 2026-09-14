#if WITH_DEV_AUTOMATION_TESTS

#include "Gameplay/Relic/Components/NPThrowableRelicComponent.h"
#include "Misc/AutomationTest.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPThrowableRelicVelocityTest,
	"NoPhotos.Relic.Throwable.Velocity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNPThrowableRelicVelocityTest::RunTest(const FString& Parameters)
{
	FNPRelicThrowSettings Settings;
	Settings.ForwardSpeed = 1200.0f;
	Settings.UpwardSpeed = 250.0f;
	Settings.bInheritThrowerVelocity = false;
	const FVector Velocity = UNPThrowableRelicComponent::CalculateThrowVelocity(
		FVector(0.0f, 0.6f, 0.8f), FVector(100.0f, 100.0f, 100.0f), Settings);
	TestTrue(TEXT("Camera pitch is preserved and view direction is normalized"),
		Velocity.Equals(FVector(0.0f, 720.0f, 1210.0f), 0.001f));

	Settings.bInheritThrowerVelocity = true;
	const FVector Inherited = UNPThrowableRelicComponent::CalculateThrowVelocity(
		FVector::ForwardVector, FVector(100.0f, -50.0f, 25.0f), Settings);
	TestTrue(TEXT("Thrower velocity is inherited once"),
		Inherited.Equals(FVector(1300.0f, -50.0f, 275.0f), 0.001f));

	Settings.ForwardSpeed = std::numeric_limits<float>::quiet_NaN();
	Settings.UpwardSpeed = -50.0f;
	const FVector Sanitized = UNPThrowableRelicComponent::CalculateThrowVelocity(
		FVector::ZeroVector,
		FVector(std::numeric_limits<float>::infinity(), 0.0f, 0.0f),
		Settings);
	TestTrue(TEXT("Invalid and negative settings produce a finite safe velocity"),
		Sanitized.IsNearlyZero() && !Sanitized.ContainsNaN());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPThrowableRelicSpinTest,
	"NoPhotos.Relic.Throwable.Spin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNPThrowableRelicSpinTest::RunTest(const FString& Parameters)
{
	FNPRelicThrowSettings Settings;
	Settings.LocalSpinAxis = FVector::YAxisVector;
	Settings.SpinSpeed = 1440.0f;
	const FTransform RotatedRelic(FRotator(0.0f, 90.0f, 0.0f));
	const FVector Angular = UNPThrowableRelicComponent::CalculateAngularVelocityDegrees(
		RotatedRelic, Settings);
	TestTrue(TEXT("Local spin axis follows relic world rotation"),
		Angular.Equals(FVector(-1440.0f, 0.0f, 0.0f), 0.01f));

	Settings.LocalSpinAxis = FVector::ZeroVector;
	Settings.SpinSpeed = -1.0f;
	const FVector Stopped = UNPThrowableRelicComponent::CalculateAngularVelocityDegrees(
		FTransform::Identity, Settings);
	TestTrue(TEXT("Zero axis has a fallback and negative speed clamps to stopped"),
		Stopped.IsNearlyZero() && !Stopped.ContainsNaN());
	return true;
}

#endif

