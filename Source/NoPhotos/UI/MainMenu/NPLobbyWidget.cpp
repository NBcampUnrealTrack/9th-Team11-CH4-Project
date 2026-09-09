#include "UI/MainMenu/NPLobbyWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Core/Chat/NPChatComponent.h"
#include "Core/Room/NPRoomGameState.h"
#include "Core/Room/NPRoomPlayerController.h"
#include "InputCoreTypes.h"
#include "TimerManager.h"
#include "UI/MainMenu/Lobby/NPJoinPlayerList.h"

UNPLobbyWidget::UNPLobbyWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	SetInputModeState(ENPWidgetInputMode::GameOnly);
}

void UNPLobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);

	if (IsValid(StartButton))
	{
		StartButton->OnClicked.AddDynamic(this, &UNPLobbyWidget::OnStartButtonClicked);
	}

	if (IsValid(LeaveButton))
	{
		LeaveButton->OnClicked.AddDynamic(this, &UNPLobbyWidget::OnLeaveClicked);
	}

	ANPRoomGameState* RoomGameState = GetWorld() ? GetWorld()->GetGameState<ANPRoomGameState>()	: nullptr;

	if (IsValid(RoomGameState))
	{
		BoundRoomGameState = RoomGameState;
		RoomGameState->OnRoomStateChanged.AddDynamic(this, &UNPLobbyWidget::OnRoomStateChanged);
	}

	if (IsValid(JoinPlayerListWidget))
	{
		JoinPlayerListWidget->OnPlayerListChanged.AddUObject(this, &UNPLobbyWidget::OnPlayerListChanged);
	}

	OnPlayerListChanged();

	RefreshStartButtonVisibility();
}

FReply UNPLobbyWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::E)
	{
		ANPRoomPlayerController* RoomPlayerController = Cast<ANPRoomPlayerController>(GetOwningPlayer());
		UNPChatComponent* ChatComponent = RoomPlayerController ? RoomPlayerController->GetChatComponent() : nullptr;
		if (IsValid(RoomPlayerController) && (!IsValid(ChatComponent) || !ChatComponent->IsChatInputOpen()))
		{
			RoomPlayerController->ToggleLobbyInputMode();
			return FReply::Handled();
		}
	}

	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply UNPLobbyWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const bool bIsMouseOver = IsValid(JoinPlayerHoverArea)
		&& JoinPlayerHoverArea->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition());
	UpdateJoinPlayerAreaHover(bIsMouseOver);

	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void UNPLobbyWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	UpdateJoinPlayerAreaHover(false);
}

void UNPLobbyWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PlayerListCollapseTimer);
	}

	if (BoundRoomGameState.IsValid())
	{
		BoundRoomGameState->OnRoomStateChanged.RemoveDynamic(this, &UNPLobbyWidget::OnRoomStateChanged);
	}

	if (IsValid(StartButton))
	{
		StartButton->OnClicked.RemoveAll(this);
	}

	if (IsValid(LeaveButton))
	{
		LeaveButton->OnClicked.RemoveAll(this);
	}

	if (IsValid(JoinPlayerListWidget))
	{
		JoinPlayerListWidget->OnPlayerListChanged.RemoveAll(this);
	}

	BoundRoomGameState.Reset();

	Super::NativeDestruct();
}

void UNPLobbyWidget::RefreshStartButtonVisibility()
{
	if (!IsValid(StartButton))
	{
		return;
	}

	ANPRoomPlayerController* RoomPlayerController = Cast<ANPRoomPlayerController>(GetOwningPlayer());
	const bool bIsRoomHost = IsValid(RoomPlayerController) && RoomPlayerController->IsRoomHost();
	const bool bCanStartGame = BoundRoomGameState.IsValid()	&& BoundRoomGameState->CanHostStartGame();

	StartButton->SetVisibility(bIsRoomHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	StartButton->SetIsEnabled(bIsRoomHost && bCanStartGame);
}

void UNPLobbyWidget::OnRoomStateChanged()
{
	RefreshStartButtonVisibility();
}

void UNPLobbyWidget::OnPlayerListChanged()
{
	bIsPlayerListStable = false;
	ExpandPlayerList();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			PlayerListCollapseTimer,
			this,
			&UNPLobbyWidget::HandlePlayerListBecameStable,
			PlayerListStableDelay,
			false);
	}
}

void UNPLobbyWidget::HandlePlayerListBecameStable()
{
	bIsPlayerListStable = true;
	if (!bIsMouseOverJoinPlayerArea)
	{
		CollapsePlayerList();
	}
}

void UNPLobbyWidget::UpdateJoinPlayerAreaHover(const bool bIsMouseOver)
{
	if (bIsMouseOverJoinPlayerArea == bIsMouseOver)
	{
		return;
	}

	bIsMouseOverJoinPlayerArea = bIsMouseOver;
	if (bIsMouseOverJoinPlayerArea)
	{
		ExpandPlayerList();
	}
	else if (bIsPlayerListStable)
	{
		CollapsePlayerList();
	}
}

void UNPLobbyWidget::ExpandPlayerList()
{
	if (IsValid(JoinPlayers_Collapse))
	{
		PlayAnimationReverse(JoinPlayers_Collapse);
	}
}

void UNPLobbyWidget::CollapsePlayerList()
{
	if (IsValid(JoinPlayers_Collapse))
	{
		PlayAnimation(JoinPlayers_Collapse);
	}
}

void UNPLobbyWidget::OnStartButtonClicked()
{
	ANPRoomPlayerController* RoomPlayerController = Cast<ANPRoomPlayerController>(GetOwningPlayer());
	if (IsValid(RoomPlayerController) && RoomPlayerController->IsRoomHost()
		&& BoundRoomGameState.IsValid() && BoundRoomGameState->CanHostStartGame())
	{
		RoomPlayerController->RequestStartGame();
	}
}

void UNPLobbyWidget::OnLeaveClicked()
{
	if (ANPRoomPlayerController* RoomPlayerController = Cast<ANPRoomPlayerController>(GetOwningPlayer()))
	{
		RoomPlayerController->ExitRoom();
	}
}
