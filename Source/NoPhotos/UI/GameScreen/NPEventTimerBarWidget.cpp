#include "UI/GameScreen/NPEventTimerBarWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

void UNPEventTimerBarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	StopEventTimer();
}

void UNPEventTimerBarWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bIsTimerRunning)
	{
		return;
	}

	TimeUntilNextUpdate -= InDeltaTime;
	if (TimeUntilNextUpdate <= 0.0f)
	{
		UpdateTimerDisplay();
		TimeUntilNextUpdate = UpdateIntervalSeconds;
	}
}

void UNPEventTimerBarWidget::StartEventTimer(
	const float InEndServerWorldTime,
	const float InTotalDurationSeconds)
{
	TotalDurationSeconds = InTotalDurationSeconds > 0.0f ? InTotalDurationSeconds : DefaultEventDurationSeconds;
	EventEndServerWorldTime = InEndServerWorldTime > 0.0f ? InEndServerWorldTime : GetServerWorldTimeSeconds() + TotalDurationSeconds;
	bIsTimerRunning = true;
	TimeUntilNextUpdate = 0.0f;
	UpdateTimerDisplay();
}

void UNPEventTimerBarWidget::StartFallbackEventTimer()
{
	StartEventTimer(0.0f, DefaultEventDurationSeconds);
}

void UNPEventTimerBarWidget::StopEventTimer()
{
	bIsTimerRunning = false;
	EventEndServerWorldTime = 0.0f;
	TotalDurationSeconds = 0.0f;
	TimeUntilNextUpdate = 0.0f;

	if (IsValid(LeftTimeBar))
	{
		LeftTimeBar->SetPercent(0.0f);
	}
	if (IsValid(LeftTimeText))
	{
		LeftTimeText->SetText(FText::FromString(TEXT("00:00")));
	}
}

float UNPEventTimerBarWidget::GetServerWorldTimeSeconds() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0f);
}

void UNPEventTimerBarWidget::UpdateTimerDisplay()
{
	const float RemainingSeconds = FMath::Max(0.0f, EventEndServerWorldTime - GetServerWorldTimeSeconds());
	const float Progress = TotalDurationSeconds > KINDA_SMALL_NUMBER
		? FMath::Clamp(RemainingSeconds / TotalDurationSeconds, 0.0f, 1.0f)
		: 0.0f;

	if (IsValid(LeftTimeBar))
	{
		LeftTimeBar->SetPercent(Progress);
	}

	if (IsValid(LeftTimeText))
	{
		const int32 DisplaySeconds = FMath::CeilToInt(RemainingSeconds);
		const int32 Minutes = DisplaySeconds / 60;
		const int32 Seconds = DisplaySeconds % 60;
		LeftTimeText->SetText(FText::FromString(FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds)));
	}

	if (RemainingSeconds <= 0.0f)
	{
		bIsTimerRunning = false;
	}
}

