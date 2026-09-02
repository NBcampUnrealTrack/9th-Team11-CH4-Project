#include "UI/MainMenu/Room/NPRoomListWidget.h"
#include "UI/MainMenu/Room/NPRoomItemWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Core/Room/NPRoomSubsystem.h"
#include "Core/Title/NPTitlePlayerController.h"
#include "SubSystem/NPUIManagerSubsystem.h"

void UNPRoomListWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (IsValid(RefreshButton))
	{
		RefreshButton->OnClicked.AddDynamic(this, &UNPRoomListWidget::OnRefreshClicked);
	}

	if (IsValid(CloseButton))
	{
		CloseButton->OnClicked.AddDynamic(this, &UNPRoomListWidget::OnCloseClicked);
	}

	if (UGameInstance* GI = GetGameInstance())
    	{
    		if (UNPRoomSubsystem* RoomSubsystem = GI->GetSubsystem<UNPRoomSubsystem>())
    		{
    			RoomSubsystem->OnFindRoomsComplete.AddDynamic(this, &UNPRoomListWidget::OnFindRoomsComplete);
			bIsSearching = RoomSubsystem->IsFindingRooms();
    		}
    	}
    
	//미리 검색해둔 결과를 표시
	RefreshRoomList();

	//사전검색 결과가 비어있으면 재검색
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (const UNPRoomSubsystem* RoomSubsystem = GameInstance->GetSubsystem<UNPRoomSubsystem>();
			RoomSubsystem && RoomSubsystem->GetListedRooms().IsEmpty())
		{
			OnRefreshClicked();
		}
	}
}

void UNPRoomListWidget::NativeDestruct()
{
	if (IsValid(RefreshButton))
	{
		RefreshButton->OnClicked.RemoveAll(this);
	}

	if (IsValid(CloseButton))
	{
		CloseButton->OnClicked.RemoveAll(this);
	}
	
	if (UGameInstance* GI = GetGameInstance())
    {
    	if (UNPRoomSubsystem* RoomSubsystem = GI->GetSubsystem<UNPRoomSubsystem>())
    	{
    		RoomSubsystem->OnFindRoomsComplete.RemoveAll(this);
    	}
    }

	Super::NativeDestruct();
}

void UNPRoomListWidget::OnRefreshClicked()
{
	if (ANPTitlePlayerController* TitlePlayerController = Cast<ANPTitlePlayerController>(GetOwningPlayer()))
	{
		TitlePlayerController->FindRooms();
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (const UNPRoomSubsystem* RoomSubsystem = GameInstance->GetSubsystem<UNPRoomSubsystem>())
		{
			UpdateSearchState(RoomSubsystem->IsFindingRooms());
		}
	}
}

void UNPRoomListWidget::OnFindRoomsComplete(const TArray<int32>& RoomIndices)
{
	UpdateSearchState(false);
}

void UNPRoomListWidget::UpdateSearchState(const bool bInIsSearching)
{
	bIsSearching = bInIsSearching;

	if (IsValid(RefreshButton))
	{
		RefreshButton->SetIsEnabled(!bIsSearching);
	}

	RefreshRoomList();
}

void UNPRoomListWidget::RefreshRoomList()
{
	if (!IsValid(RoomListScrollBox) || !IsValid(RoomItemClass)) return;

	RoomListScrollBox->ClearChildren();

	UGameInstance* GI = GetGameInstance();
	UNPRoomSubsystem* RoomSubsystem = GI ? GI->GetSubsystem<UNPRoomSubsystem>() : nullptr;
	if (!IsValid(RoomSubsystem)) return;

	if (bIsSearching && WidgetTree)
	{
		UTextBlock* SearchingText = WidgetTree->ConstructWidget<UTextBlock>();
		if (IsValid(SearchingText))
		{
			SearchingText->SetText(FText::FromString(TEXT("~ 검색중 ~")));
			if (SearchingTextFont.FontObject)
			{
				SearchingText->SetFont(SearchingTextFont);
			}
			SearchingText->SetColorAndOpacity(FSlateColor(SearchingTextColor));
			SearchingText->SetJustification(ETextJustify::Center);
			if (UScrollBoxSlot* SearchingTextSlot = Cast<UScrollBoxSlot>(RoomListScrollBox->AddChild(SearchingText)))
			{
				SearchingTextSlot->SetHorizontalAlignment(HAlign_Fill);
			}
		}
	}

	if (bIsSearching)
	{
		return;
	}

	const TArray<FNPRoomListEntry> Rooms = RoomSubsystem->GetListedRooms();
	for (const FNPRoomListEntry& Room : Rooms)
	{
		UNPRoomItemWidget* ItemWidget = CreateWidget<UNPRoomItemWidget>(this, RoomItemClass);
		if (IsValid(ItemWidget))
		{
			ItemWidget->SetupRoomInfo(
				Room.RoomNumber,
				Room.HostName,
				Room.CurrentPlayers,
				Room.MaxPlayers);
			ItemWidget->OnRoomSelected.AddDynamic(this, &UNPRoomListWidget::OnRoomItemSelected);
			RoomListScrollBox->AddChild(ItemWidget);
		}
	}
}

void UNPRoomListWidget::OnRoomItemSelected(int32 SelectedRoomNumber)
{
	if (ANPTitlePlayerController* TitlePlayerController = Cast<ANPTitlePlayerController>(GetOwningPlayer()))
	{
		TitlePlayerController->JoinRoom(SelectedRoomNumber);
	}
}

void UNPRoomListWidget::OnCloseClicked()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UNPUIManagerSubsystem* UIManager = GI->GetSubsystem<UNPUIManagerSubsystem>())
		{
			UIManager->RequestPopWidget();
		}
	}
}
