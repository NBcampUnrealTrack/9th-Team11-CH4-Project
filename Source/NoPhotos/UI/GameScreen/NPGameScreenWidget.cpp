#include "UI/GameScreen/NPGameScreenWidget.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Gameplay/MapEvents/NPMapEventManager.h"
#include "TimerManager.h"
#include "UI/GameScreen/NPEventTimerBarWidget.h"

void UNPGameScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetEventTimerVisible(false);
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

void UNPGameScreenWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EventManagerBindRetryTimerHandle);
	}

	UnbindFromEventManager();
	Super::NativeDestruct();
}

void UNPGameScreenWidget::HandleActiveMapEventsChanged()
{
	RefreshEventTimer();
}

bool UNPGameScreenWidget::TryBindToEventManager()
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

	RefreshEventTimer();
	return true;
}

void UNPGameScreenWidget::RetryBindToEventManager()
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

void UNPGameScreenWidget::UnbindFromEventManager()
{
	if (UNPMapEventManagerComponent* EventManager = BoundEventManager.Get())
	{
		EventManager->OnActiveMapEventsChanged.RemoveAll(this);
	}

	BoundEventManager.Reset();
}

void UNPGameScreenWidget::RefreshEventTimer()
{
	UNPMapEventManagerComponent* EventManager = BoundEventManager.Get();
	if (!IsValid(EventManager) || !IsValid(EventTimerBar))
	{
		if (IsValid(EventInfo))
		{
			EventInfo->SetText(FText::GetEmpty());
		}

		SetEventTimerVisible(false);
		return;
	}

	FNPActiveMapEventPresentation Presentation;
	if (!EventManager->GetPrimaryActiveEventPresentation(Presentation))
	{
		EventTimerBar->StopEventTimer();
		if (IsValid(EventInfo))
		{
			EventInfo->SetText(FText::GetEmpty());
		}

		SetEventTimerVisible(false);
		return;
	}

	EventTimerBar->StartEventTimer(Presentation.EndServerWorldTime, Presentation.DurationSeconds);
	if (IsValid(EventInfo))
	{
		EventInfo->SetText(Presentation.Description);
	}

	SetEventTimerVisible(true);
}

void UNPGameScreenWidget::SetEventTimerVisible(const bool bVisible)
{
	const ESlateVisibility TargetVisibility = bVisible
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed;

	if (IsValid(EventTimerBar))
	{
		EventTimerBar->SetVisibility(TargetVisibility);
	}

	if (IsValid(EventInfo))
	{
		EventInfo->SetVisibility(TargetVisibility);
	}
}
