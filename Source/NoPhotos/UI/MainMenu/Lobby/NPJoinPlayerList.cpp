#include "UI/MainMenu/Lobby/NPJoinPlayerList.h"

#include "NPJoinPlayer.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Core/NPPlayerState.h"
#include "Core/Room/NPRoomGameState.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

void UNPJoinPlayerList::NativeConstruct()
{
	Super::NativeConstruct();

	RefreshPlayerList();
}

void UNPJoinPlayerList::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PlayerNameRefreshTimer);
	}

	if (ANPRoomGameState* RoomGameState = BoundRoomGameState.Get())
	{
		RoomGameState->OnRoomStateChanged.RemoveAll(this);
	}
	BoundRoomGameState.Reset();

	Super::NativeDestruct();
}

void UNPJoinPlayerList::OnRoomStateChanged()
{
	RefreshPlayerList();
}

void UNPJoinPlayerList::RefreshPlayerList()
{
    if (!IsValid(PlayerList) || !IsValid(JoinPlayerWidgetClass))
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!IsValid(World))
    {
        return;
    }

    ANPRoomGameState* RoomGameState = World->GetGameState<ANPRoomGameState>();
    if (!IsValid(RoomGameState))
    {
        World->GetTimerManager().SetTimer(
            PlayerNameRefreshTimer,
            this,
            &UNPJoinPlayerList::RefreshPlayerList,
            0.1f,
            false);

        return;
    }

	if (BoundRoomGameState.Get() != RoomGameState)
	{
		if (ANPRoomGameState* PreviousGameState = BoundRoomGameState.Get())
		{
			PreviousGameState->OnRoomStateChanged.RemoveAll(this);
		}

		BoundRoomGameState = RoomGameState;
		RoomGameState->OnRoomStateChanged.AddUniqueDynamic(this, &UNPJoinPlayerList::OnRoomStateChanged);
	}

    const TArray<FNPPlayerRoomInfo> PlayerMemberList = RoomGameState->GetRoomMembers();
    bool bNeedRefreshAgain = PlayerMemberList.IsEmpty();
	TSet<TObjectPtr<APlayerState>> CurrentPlayerStates;
	
    APlayerState* LocalPlayerState = GetOwningPlayer() ? GetOwningPlayer()->PlayerState : nullptr;
    bool bLocalPlayerInMemberList = false;
    if (!IsValid(LocalPlayerState))
    {
        bNeedRefreshAgain = true;
    }

    for (int32 PlayerIndex = 0; PlayerIndex < PlayerMemberList.Num(); ++PlayerIndex)
    {
        const FNPPlayerRoomInfo& RoomMember = PlayerMemberList[PlayerIndex];
        if (RoomMember.PlayerState == LocalPlayerState)
		{
			bLocalPlayerInMemberList = true;
		}
        FString PlayerName = TEXT("접속 중...");

        if (IsValid(RoomMember.PlayerState))
        {
            PlayerName = RoomMember.PlayerState->GetPlayerName();
            
            if (PlayerName.IsEmpty())
            {
                PlayerName = TEXT("접속 중...");
                bNeedRefreshAgain = true;
            }
        }
        else
        {
            bNeedRefreshAgain = true;
			continue;
        }

		APlayerState* PlayerState = RoomMember.PlayerState;
		CurrentPlayerStates.Add(PlayerState);

		if (UNPJoinPlayer* ExistingPlayerWidget = PlayerWidgets.FindRef(PlayerState))
		{
			ExistingPlayerWidget->SetupResult(PlayerName, PlayerState);
			continue;
		}

        UNPJoinPlayer* JoinPlayerWidget = CreateWidget<UNPJoinPlayer>(GetOwningPlayer(), JoinPlayerWidgetClass);
        if (!IsValid(JoinPlayerWidget))
        {
            continue;
        }

		JoinPlayerWidget->SetupResult(PlayerName, PlayerState);
        JoinPlayerWidget->SetRenderTransformAngle(FMath::FRandRange(-4.0f, 4.0f));

        UHorizontalBoxSlot* PlayerSlot = PlayerList->AddChildToHorizontalBox(JoinPlayerWidget);
        if (IsValid(PlayerSlot))
        {
            PlayerSlot->SetPadding(FMargin(0.0f, 0.0f, -40.0f, -30.0f));
        }

		PlayerWidgets.Add(PlayerState, JoinPlayerWidget);
		const float JoinAnimationDelay = bInitialPlayerPopulationComplete ? 0.0f : InitialJoinAnimationIndex++ * InitialJoinAnimationInterval;
		JoinPlayerWidget->PlayJoinAnimation(JoinAnimationDelay);
    }

    if (IsValid(LocalPlayerState) && !bLocalPlayerInMemberList)
    {
        bNeedRefreshAgain = true;
    }
    if (bNeedRefreshAgain)
    {
        World->GetTimerManager().SetTimer(
            PlayerNameRefreshTimer,
            this,
            &UNPJoinPlayerList::RefreshPlayerList,
            0.1f,
            false);

        return;
    }

	bInitialPlayerPopulationComplete = true;
    for (auto PlayerWidgetIterator = PlayerWidgets.CreateIterator(); PlayerWidgetIterator; ++PlayerWidgetIterator)
    {
    	if (CurrentPlayerStates.Contains(PlayerWidgetIterator.Key()))
    	{
    		continue;
    	}
    
    	if (UNPJoinPlayer* PlayerWidget = PlayerWidgetIterator.Value())
		{
			if (!PlayerWidget->IsLeaving())
			{
				LeavingPlayerWidgets.Add(PlayerWidget);
				PlayerWidget->OnLeaveAnimationFinished.AddUObject(
					this,
					&UNPJoinPlayerList::OnPlayerLeaveAnimationFinished);
				PlayerWidget->PlayLeaveAnimation();
			}
		}
		PlayerWidgetIterator.RemoveCurrent();
	}

    World->GetTimerManager().ClearTimer(PlayerNameRefreshTimer);
}

void UNPJoinPlayerList::OnPlayerLeaveAnimationFinished(UNPJoinPlayer* PlayerWidget)
{
	if (!IsValid(PlayerWidget) || !LeavingPlayerWidgets.Contains(PlayerWidget))
	{
		return;
	}

	PlayerWidget->OnLeaveAnimationFinished.RemoveAll(this);
	PlayerWidget->RemoveFromParent();
	LeavingPlayerWidgets.Remove(PlayerWidget);
}
