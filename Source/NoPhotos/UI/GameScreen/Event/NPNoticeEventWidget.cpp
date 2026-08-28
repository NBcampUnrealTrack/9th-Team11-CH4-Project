#include "UI/GameScreen/Event/NPNoticeEventWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Gameplay/MapEvents/NPMapEventManager.h"
#include "TimerManager.h"

void UNPNoticeEventWidget::NativeConstruct()
{
	Super::NativeConstruct();

	HideEventNotice();
	if (TryBindToEventManager())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			EventManagerBindRetryTimerHandle,
			this,
			&ThisClass::RetryBindToEventManager,
			0.1f,
			true);
	}
}

void UNPNoticeEventWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EventManagerBindRetryTimerHandle);
	}

	UnbindFromEventManager();
	Super::NativeDestruct();
}

void UNPNoticeEventWidget::ShowEventNotice(const FText& EventTitle)
{
	if (IsValid(EventNameText))
	{
		EventNameText->SetText(FText::Format(NSLOCTEXT("MapEvent", "EventOccurredNotice", "{0} 발생!"), EventTitle));
	}

	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (IsValid(NoticeAnimation))
	{
		PlayAnimation(NoticeAnimation);
	}
}

void UNPNoticeEventWidget::HideEventNotice()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

void UNPNoticeEventWidget::OnAnimationFinished_Implementation(const UWidgetAnimation* Animation)
{
	Super::OnAnimationFinished_Implementation(Animation);

	if (Animation==NoticeAnimation)
	{
		HideEventNotice();
	}
}

void UNPNoticeEventWidget::HandleActiveMapEventsChanged()
{
	RefreshActiveEventNotices(true);
}

bool UNPNoticeEventWidget::TryBindToEventManager()
{
	UWorld* World = GetWorld();
	AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	UNPMapEventManagerComponent* EventManager = GameState ? GameState->FindComponentByClass<UNPMapEventManagerComponent>() : nullptr;
	if (!IsValid(EventManager))
	{
		return false;
	}

	if (BoundEventManager.Get() != EventManager)
	{
		UnbindFromEventManager();
		BoundEventManager = EventManager;
		EventManager->OnActiveMapEventsChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleActiveMapEventsChanged);
	}

	//뒤늦게 화면이 뜰 경우, 이벤트가 진행 중이었으면 알림을 띄움
	RefreshActiveEventNotices(true);
	return true;
}

void UNPNoticeEventWidget::RetryBindToEventManager()
{
	if (!TryBindToEventManager())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EventManagerBindRetryTimerHandle);
	}
}

void UNPNoticeEventWidget::UnbindFromEventManager()
{
	if (UNPMapEventManagerComponent* EventManager = BoundEventManager.Get())
	{
		EventManager->OnActiveMapEventsChanged.RemoveAll(this);
	}

	BoundEventManager.Reset();
	KnownActiveEventIds.Reset();
}

void UNPNoticeEventWidget::RefreshActiveEventNotices(const bool bShowNewNotices)
{
	UNPMapEventManagerComponent* EventManager = BoundEventManager.Get();
	if (!IsValid(EventManager))
	{
		return;
	}

	const TArray<FNPActiveMapEventPresentation> ActiveEvents = EventManager->GetActiveEventPresentations();
	TSet<FName> CurrentActiveEventIds;
	const FNPActiveMapEventPresentation* MostRecentNewEvent = nullptr;

	for (const FNPActiveMapEventPresentation& Presentation : ActiveEvents)
	{
		if (Presentation.EventId.IsNone())
		{
			continue;
		}

		CurrentActiveEventIds.Add(Presentation.EventId);
		if (bShowNewNotices && !KnownActiveEventIds.Contains(Presentation.EventId))
		{
			// 매니저는 가장 최근에 시작된 이벤트를 배열 끝에 추가합니다.
			MostRecentNewEvent = &Presentation;
		}
	}

	KnownActiveEventIds = MoveTemp(CurrentActiveEventIds);
	if (MostRecentNewEvent)
	{
		ShowEventNotice(MostRecentNewEvent->Title);
	}
}

