#include "UI/GameScreen/Event/NPEventProgressWidget.h"

#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Core/Main/NPMainGameState.h"
#include "Engine/World.h"
#include "TimerManager.h"

UNPEventProgressWidget::UNPEventProgressWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UNPEventProgressWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!TryBindToGameState())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(GameStateBindRetryTimerHandle, this,
				&ThisClass::RetryBindToGameState, 0.1f, true);
		}
	}

	UpdateProgressBar();
	RebuildEventMarkers();
}

void UNPEventProgressWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GameStateBindRetryTimerHandle);
	}

	UnbindFromGameState();
	Super::NativeDestruct();
}

void UNPEventProgressWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateProgressBar();

	if (IsValid(EventMarkerBox))
	{
		const float MarkerBoxWidth = EventMarkerBox->GetCachedGeometry().GetLocalSize().X;
		if (MarkerBoxWidth > KINDA_SMALL_NUMBER && !FMath::IsNearlyEqual(MarkerBoxWidth, LastMarkerBoxWidth))
		{
			RebuildEventMarkers();
		}
	}
}

void UNPEventProgressWidget::HandleMainGameStateChanged()
{
	UpdateProgressBar();
	RebuildEventMarkers();
}

void UNPEventProgressWidget::HandleEventScheduleChanged()
{
	RebuildEventMarkers();
}

bool UNPEventProgressWidget::TryBindToGameState()
{
	UWorld* World = GetWorld();
	ANPMainGameState* MainGameState = World ? World->GetGameState<ANPMainGameState>() : nullptr;
	if (!IsValid(MainGameState))
	{
		return false;
	}

	if (BoundMainGameState.Get() != MainGameState)
	{
		UnbindFromGameState();
		BoundMainGameState = MainGameState;
		MainGameState->OnMainGameStateChanged.AddUniqueDynamic(this, &ThisClass::HandleMainGameStateChanged);
	}

	UNPMapEventManagerComponent* EventManager = MainGameState->FindComponentByClass<UNPMapEventManagerComponent>();
	if (BoundEventManager.Get() != EventManager)
	{
		if (UNPMapEventManagerComponent* PreviousEventManager = BoundEventManager.Get())
		{
			PreviousEventManager->OnEventScheduleChanged.RemoveAll(this);
		}

		BoundEventManager = EventManager;
		if (IsValid(EventManager))
		{
			EventManager->OnEventScheduleChanged.AddUniqueDynamic(this, &ThisClass::HandleEventScheduleChanged);
		}
	}

	HandleMainGameStateChanged();
	HandleEventScheduleChanged();
	return IsValid(EventManager);
}

void UNPEventProgressWidget::RetryBindToGameState()
{
	if (!TryBindToGameState())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GameStateBindRetryTimerHandle);
	}
}

void UNPEventProgressWidget::UnbindFromGameState()
{
	if (ANPMainGameState* MainGameState = BoundMainGameState.Get())
	{
		MainGameState->OnMainGameStateChanged.RemoveAll(this);
	}
	if (UNPMapEventManagerComponent* EventManager = BoundEventManager.Get())
	{
		EventManager->OnEventScheduleChanged.RemoveAll(this);
	}

	BoundMainGameState.Reset();
	BoundEventManager.Reset();
}

void UNPEventProgressWidget::UpdateProgressBar()
{
	if (!IsValid(GameProgressBar))
	{
		return;
	}

	const ANPMainGameState* MainGameState = BoundMainGameState.Get();
	if (!IsValid(MainGameState))
	{
		GameProgressBar->SetPercent(0.0f);
		return;
	}

	const float RemainingSeconds = FMath::Max(0, MainGameState->GetRemainingGameTime());
	if (GameDurationSeconds <= KINDA_SMALL_NUMBER && MainGameState->IsMainGameActive())
	{
		ObservedGameDurationSeconds = FMath::Max(ObservedGameDurationSeconds, RemainingSeconds);
	}

	const float TotalSeconds = GetResolvedGameDurationSeconds();
	GameProgressBar->SetPercent(TotalSeconds > KINDA_SMALL_NUMBER
		? FMath::Clamp(1.0f - RemainingSeconds / TotalSeconds, 0.0f, 1.0f)
		: 0.0f);
	GameProgressBar->SetFillColorAndOpacity(FMath::Lerp(
		NormalProgressColor,
		NearEventProgressColor,
		GetNextEventWarningAlpha()));
}

float UNPEventProgressWidget::GetNextEventWarningAlpha() const
{
	const UNPMapEventManagerComponent* EventManager = BoundEventManager.Get();
	if (!IsValid(EventManager) || EventWarningLeadTimeSeconds <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	const FNPMapEventSchedulePresentation Schedule = EventManager->GetEventSchedule();
	const float ElapsedSeconds = EventManager->GetScheduleElapsedSeconds();
	for (const FNPScheduledMapEventPresentation& Event : Schedule.Events)
	{
		if ((Event.State != ENPScheduledMapEventState::Pending && Event.State != ENPScheduledMapEventState::Loading)
			|| Event.ExpectedStartServerWorldTime < 0.0f)
		{
			continue;
		}

		const float RemainingSeconds = Event.ExpectedStartServerWorldTime
			- Schedule.ScheduleOriginServerWorldTime - ElapsedSeconds;
		return 1.0f - FMath::Clamp(
			RemainingSeconds / EventWarningLeadTimeSeconds,
			0.0f,
			1.0f);
	}

	return 0.0f;
}

void UNPEventProgressWidget::RebuildEventMarkers()
{
	if (!IsValid(EventMarkerBox))
	{
		return;
	}

	EventMarkerBox->ClearChildren();
	const UNPMapEventManagerComponent* EventManager = BoundEventManager.Get();
	const float TotalSeconds = GetResolvedGameDurationSeconds();
	if (!IsValid(EventManager) || TotalSeconds <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FNPMapEventSchedulePresentation Schedule = EventManager->GetEventSchedule();
	const float MarkerBoxWidth = EventMarkerBox->GetCachedGeometry().GetLocalSize().X;
	LastMarkerBoxWidth = MarkerBoxWidth;
	const bool bUsePixelAccurateImageLayout = IsValid(EventMarkerImage) && MarkerBoxWidth > KINDA_SMALL_NUMBER;
	const float MarkerSlotWidth = FMath::Max(0.0f,
		EventMarkerImageSize.X + EventMarkerHorizontalPadding * 2.0f);
	const float MarkerCenterOffset = EventMarkerHorizontalPadding + EventMarkerImageSize.X * 0.5f;
	float PreviousProgress = 0.0f;
	float PreviousMarkerSlotRight = 0.0f;
	for (const FNPScheduledMapEventPresentation& Event : Schedule.Events)
	{
		if (Event.ExpectedStartServerWorldTime < 0.0f)
		{
			continue;
		}

		const float EventProgress = FMath::Clamp(
			(Event.ExpectedStartServerWorldTime - Schedule.ScheduleOriginServerWorldTime) / TotalSeconds,
			0.0f, 1.0f);
		if (bUsePixelAccurateImageLayout)
		{
			const float MarkerSlotLeft = EventProgress * MarkerBoxWidth - MarkerCenterOffset
				+ EventMarkerTriggerOffsetPixels;
			AddFillSpacer(FMath::Max(0.0f, MarkerSlotLeft - PreviousMarkerSlotRight));
			PreviousMarkerSlotRight = MarkerSlotLeft + MarkerSlotWidth;
		}
		else
		{
			AddFillSpacer(FMath::Max(0.0f, EventProgress - PreviousProgress));
		}

		UWidget* Marker = nullptr;
		if (IsValid(EventMarkerImage))
		{
			USizeBox* ImageSizeBox = NewObject<USizeBox>(this);
			ImageSizeBox->SetWidthOverride(EventMarkerImageSize.X);
			ImageSizeBox->SetHeightOverride(EventMarkerImageSize.Y);

			UImage* ImageMarker = NewObject<UImage>(ImageSizeBox);
			ImageMarker->SetBrushFromTexture(EventMarkerImage);
			ImageMarker->SetToolTipText(Event.Title);
			ImageSizeBox->SetContent(ImageMarker);
			Marker = ImageSizeBox;
		}
		if (!IsValid(Marker))
		{
			UTextBlock* TextMarker = NewObject<UTextBlock>(this);
			TextMarker->SetText(DefaultMarkerText);
			TextMarker->SetToolTipText(Event.Title);
			TextMarker->SetJustification(ETextJustify::Center);
			Marker = TextMarker;
		}
		const bool bShowMarker = Event.State == ENPScheduledMapEventState::Pending
			|| Event.State == ENPScheduledMapEventState::Loading;
		Marker->SetRenderOpacity(bShowMarker ? 1.0f : 0.0f);

		if (UHorizontalBoxSlot* MarkerSlot = EventMarkerBox->AddChildToHorizontalBox(Marker))
		{
			MarkerSlot->SetVerticalAlignment(VAlign_Center);
			MarkerSlot->SetPadding(FMargin(
				EventMarkerHorizontalPadding,
				0.0f,
				EventMarkerHorizontalPadding,
				0.0f));
		}
		PreviousProgress = EventProgress;
	}

	AddFillSpacer(bUsePixelAccurateImageLayout
		? FMath::Max(0.0f, MarkerBoxWidth - PreviousMarkerSlotRight)
		: FMath::Max(0.0f, 1.0f - PreviousProgress));
}

float UNPEventProgressWidget::GetResolvedGameDurationSeconds() const
{
	return GameDurationSeconds > KINDA_SMALL_NUMBER ? GameDurationSeconds : ObservedGameDurationSeconds;
}

void UNPEventProgressWidget::AddFillSpacer(const float FillWeight)
{
	if (!IsValid(EventMarkerBox) || FillWeight <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	USpacer* Spacer = NewObject<USpacer>(this);
	if (UHorizontalBoxSlot* SpacerSlot = EventMarkerBox->AddChildToHorizontalBox(Spacer))
	{
		FSlateChildSize FillSize;
		FillSize.SizeRule = ESlateSizeRule::Fill;
		FillSize.Value = FillWeight;
		SpacerSlot->SetSize(FillSize);
	}
}

