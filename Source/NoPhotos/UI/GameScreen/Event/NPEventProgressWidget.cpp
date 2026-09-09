#include "UI/GameScreen/Event/NPEventProgressWidget.h"

#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Core/Main/NPMainGameState.h"
#include "Engine/World.h"
#include "Animation/WidgetAnimation.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/Widget.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"

UNPEventProgressWidget::UNPEventProgressWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FClassFinder<UUserWidget> EventMarkerWidgetFinder(
		TEXT("/Game/NoPhotos/Blueprints/UI/GameScreen/Event/WBP_EventMarker"));
	if (EventMarkerWidgetFinder.Succeeded())
	{
		EventMarkerWidgetClass = EventMarkerWidgetFinder.Class;
	}
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
	EventMarkers.Empty();
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

	const UNPMapEventManagerComponent* EventManager = BoundEventManager.Get();
	const float TotalSeconds = GetResolvedGameDurationSeconds();
	if (!IsValid(EventManager) || TotalSeconds <= KINDA_SMALL_NUMBER)
	{
		EventMarkerBox->ClearChildren();
		EventMarkers.Empty();
		return;
	}

	const FNPMapEventSchedulePresentation Schedule = EventManager->GetEventSchedule();
	EventMarkerBox->ClearChildren();
	const float MarkerBoxWidth = EventMarkerBox->GetCachedGeometry().GetLocalSize().X;
	LastMarkerBoxWidth = MarkerBoxWidth;
	const bool bUsePixelAccurateImageLayout = MarkerBoxWidth > KINDA_SMALL_NUMBER;
	const float MarkerSlotWidth = FMath::Max(0.0f,
		EventMarkerImageSize.X + EventMarkerHorizontalPadding * 2.0f);
	const float MarkerCenterOffset = EventMarkerHorizontalPadding + EventMarkerImageSize.X * 0.5f;
	float PreviousProgress = 0.0f;
	float PreviousMarkerSlotRight = 0.0f;
	TSet<int32> CurrentScheduleIndices;
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

		FEventMarkerRuntime* Marker = EventMarkers.Find(Event.ScheduleIndex);
		const bool bIsNewMarker = Marker == nullptr || !Marker->Widget.IsValid() || !Marker->SizeBox.IsValid();
		if (bIsNewMarker)
		{
			Marker = CreateEventMarker(Event.ScheduleIndex, Event.Title);
		}

		if (Marker == nullptr)
		{
			continue;
		}

		SetEventMarkerState(*Marker, Event.State, !bIsNewMarker);
		CurrentScheduleIndices.Add(Event.ScheduleIndex);

		if (UHorizontalBoxSlot* MarkerSlot = EventMarkerBox->AddChildToHorizontalBox(Marker->SizeBox.Get()))
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

	for (auto It = EventMarkers.CreateIterator(); It; ++It)
	{
		if (!CurrentScheduleIndices.Contains(It.Key()))
		{
			It.RemoveCurrent();
		}
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

UNPEventProgressWidget::FEventMarkerRuntime* UNPEventProgressWidget::CreateEventMarker(
	const int32 ScheduleIndex, const FText& Title)
{
	if (!EventMarkerWidgetClass)
	{
		return nullptr;
	}

	UUserWidget* MarkerWidget = CreateWidget<UUserWidget>(this, EventMarkerWidgetClass);
	if (!IsValid(MarkerWidget))
	{
		return nullptr;
	}

	USizeBox* MarkerSizeBox = NewObject<USizeBox>(this);
	MarkerSizeBox->SetWidthOverride(EventMarkerImageSize.X);
	MarkerSizeBox->SetHeightOverride(EventMarkerImageSize.Y);
	MarkerSizeBox->SetToolTipText(Title);
	MarkerSizeBox->SetContent(MarkerWidget);

	FEventMarkerRuntime& Marker = EventMarkers.FindOrAdd(ScheduleIndex);
	Marker.Widget = MarkerWidget;
	Marker.SizeBox = MarkerSizeBox;
	return &Marker;
}

void UNPEventProgressWidget::SetEventMarkerState(FEventMarkerRuntime& Marker,
	const ENPScheduledMapEventState NewState, const bool bPlayTransition)
{
	UUserWidget* MarkerWidget = Marker.Widget.Get();
	if (!IsValid(MarkerWidget))
	{
		return;
	}

	UWidget* EventIcon = FindMarkerWidget(MarkerWidget, TEXT("EventIcon"));
	UWidget* EventIconLight = FindMarkerWidget(MarkerWidget, TEXT("EventIconLight"));
	const bool bIsActive = NewState == ENPScheduledMapEventState::Active;
	const bool bWasActive = Marker.State == ENPScheduledMapEventState::Active;
	if (bIsActive)
	{
		if (IsValid(EventIcon))
		{
			EventIcon->SetRenderOpacity(1.0f);
		}
		if (IsValid(EventIconLight))
		{
			EventIconLight->SetRenderOpacity(1.0f);
		}
		if (!bWasActive || !bPlayTransition)
		{
			if (UWidgetAnimation* LightAnimation = FindMarkerAnimation(MarkerWidget, TEXT("LightAnim")))
			{
				// NumLoopsToPlay가 0이면 이벤트가 활성 상태인 동안 계속 반복됩니다.
				MarkerWidget->PlayAnimation(LightAnimation, 0.0f, 0);
			}
		}
	}
	else
	{
		if (UWidgetAnimation* LightAnimation = FindMarkerAnimation(MarkerWidget, TEXT("LightAnim")))
		{
			MarkerWidget->StopAnimation(LightAnimation);
		}
		if (IsValid(EventIconLight))
		{
			EventIconLight->SetRenderOpacity(0.0f);
		}

		const bool bHasFinished = NewState == ENPScheduledMapEventState::Completed
			|| NewState == ENPScheduledMapEventState::Cancelled;
		if (bHasFinished)
		{
			if (bWasActive && bPlayTransition)
			{
				if (UWidgetAnimation* IconOffAnimation = FindMarkerAnimation(MarkerWidget, TEXT("IconOffAnim")))
				{
					MarkerWidget->PlayAnimation(IconOffAnimation);
					if (UWorld* World = GetWorld())
					{
						const TWeakObjectPtr<UUserWidget> WeakMarkerWidget = MarkerWidget;
						FTimerHandle IconOffCompletionTimerHandle;
						World->GetTimerManager().SetTimer(IconOffCompletionTimerHandle, FTimerDelegate::CreateWeakLambda(this,
							[WeakMarkerWidget]()
							{
								if (UUserWidget* FinishedMarkerWidget = WeakMarkerWidget.Get())
								{
									if (UWidget* FinishedEventIcon = FindMarkerWidget(FinishedMarkerWidget, TEXT("EventIcon")))
									{
										FinishedEventIcon->SetRenderOpacity(0.0f);
									}
								}
							}
						), FMath::Max(0.0f, IconOffAnimation->GetEndTime() - IconOffAnimation->GetStartTime()), false);
					}
				}
				else if (IsValid(EventIcon))
				{
					EventIcon->SetRenderOpacity(0.0f);
				}
			}
			else if (IsValid(EventIcon))
			{
				EventIcon->SetRenderOpacity(0.0f);
			}
		}
	}

	Marker.State = NewState;
}

UWidget* UNPEventProgressWidget::FindMarkerWidget(const UUserWidget* MarkerWidget, const FName WidgetName)
{
	return IsValid(MarkerWidget) ? MarkerWidget->GetWidgetFromName(WidgetName) : nullptr;
}

UWidgetAnimation* UNPEventProgressWidget::FindMarkerAnimation(const UUserWidget* MarkerWidget, const FName AnimationName)
{
	if (!IsValid(MarkerWidget))
	{
		return nullptr;
	}

	const UWidgetBlueprintGeneratedClass* MarkerClass = Cast<UWidgetBlueprintGeneratedClass>(MarkerWidget->GetClass());
	if (!IsValid(MarkerClass))
	{
		return nullptr;
	}

	for (UWidgetAnimation* Animation : MarkerClass->Animations)
	{
		if (!IsValid(Animation))
		{
			continue;
		}

		const FString RuntimeAnimationName = Animation->GetName();
		const FString RequestedAnimationName = AnimationName.ToString();
		if (RuntimeAnimationName == RequestedAnimationName
			|| RuntimeAnimationName.StartsWith(RequestedAnimationName + TEXT("_")))
		{
			return Animation;
		}
	}
	return nullptr;
}

