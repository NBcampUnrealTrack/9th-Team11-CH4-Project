#include "UI/MainMenu/NPMainMenuWidget.h"
#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Core/Room/NPRoomSubsystem.h"
#include "Core/Title/NPTitlePlayerController.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"
#include "SubSystem/NPUIManagerSubsystem.h"
#include "TimerManager.h"

UNPMainMenuWidget::UNPMainMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}

void UNPMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	if (IsValid(HostButton))
	{
		HostButton->OnClicked.AddDynamic(this, &UNPMainMenuWidget::OnHostGameClicked);
	}

	if (IsValid(JoinButton))
	{
		JoinButton->OnClicked.AddDynamic(this, &UNPMainMenuWidget::OnJoinGameClicked);
	}

	if (IsValid(ExitButton))
	{
		ExitButton->OnClicked.AddDynamic(this, &UNPMainMenuWidget::OnExitClicked);
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UNPRoomSubsystem* RoomSubsystem = GameInstance->GetSubsystem<UNPRoomSubsystem>())
		{
			RoomSubsystem->RecoverSessionState();

			FText FailureMessage;
			if (RoomSubsystem->ConsumeConnectionFailureMessage(FailureMessage))
			{
				ShowConnectionFailureMessage(FailureMessage);
			}
		}
	}

	// 미리 방 검색
	if (ANPTitlePlayerController* TitlePlayerController = Cast<ANPTitlePlayerController>(GetOwningPlayer()))
	{
		TitlePlayerController->FindRooms();
	}

	CurrentCarouselImageIndex = 0;
	PlayNextCarouselImage();
}

void UNPMainMenuWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ConnectionFailureMessageTimer);
	}

	if (IsValid(HostButton))
	{
		HostButton->OnClicked.RemoveAll(this);
	}

	if (IsValid(JoinButton))
	{
		JoinButton->OnClicked.RemoveAll(this);
	}

	if (IsValid(ExitButton))
	{
		ExitButton->OnClicked.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UNPMainMenuWidget::OnAnimationFinished_Implementation(const UWidgetAnimation* Animation)
{
	Super::OnAnimationFinished_Implementation(Animation);

	if (Animation == ImageAnim)
	{
		PlayNextCarouselImage();
	}
}

void UNPMainMenuWidget::OnHostGameClicked()
{
	if (ANPTitlePlayerController* TitlePlayerController = Cast<ANPTitlePlayerController>(GetOwningPlayer()))
	{
		TitlePlayerController->HostRoom();
	}
}

void UNPMainMenuWidget::OnJoinGameClicked()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UNPUIManagerSubsystem* UIManager = GI->GetSubsystem<UNPUIManagerSubsystem>())
		{
			if (IsValid(RoomListWidgetClass))
			{
				UIManager->PushWidget(RoomListWidgetClass);
			}
		}
	}
}

void UNPMainMenuWidget::OnExitClicked()
{
	APlayerController* PC = GetOwningPlayer();
	UKismetSystemLibrary::QuitGame(GetWorld(), PC, EQuitPreference::Quit, false);
}

void UNPMainMenuWidget::PlayNextCarouselImage()
{
	if (!IsValid(Image) || CarouselImages.IsEmpty())
	{
		return;
	}

	CurrentCarouselImageIndex %= CarouselImages.Num();
	Image->SetBrushFromTexture(CarouselImages[CurrentCarouselImageIndex], true);
	CurrentCarouselImageIndex = (CurrentCarouselImageIndex + 1) % CarouselImages.Num();

	if (IsValid(ImageAnim))
	{
		PlayAnimation(ImageAnim);
	}
}

void UNPMainMenuWidget::ShowConnectionFailureMessage(const FText& Message)
{
	UCanvasPanel* RootCanvas = WidgetTree
		? Cast<UCanvasPanel>(WidgetTree->RootWidget)
		: nullptr;
	if (!RootCanvas)
	{
		return;
	}

	ConnectionFailureBanner = WidgetTree->ConstructWidget<UBorder>();
	UTextBlock* MessageText = WidgetTree->ConstructWidget<UTextBlock>();
	if (!ConnectionFailureBanner || !MessageText)
	{
		ConnectionFailureBanner = nullptr;
		return;
	}

	ConnectionFailureBanner->SetBrushColor(FLinearColor(0.35f, 0.02f, 0.02f, 0.95f));
	ConnectionFailureBanner->SetPadding(FMargin(24.0f, 12.0f));
	ConnectionFailureBanner->SetContent(MessageText);
	MessageText->SetText(Message);
	MessageText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	MessageText->SetJustification(ETextJustify::Center);
	MessageText->SetAutoWrapText(true);
	FSlateFontInfo Font = MessageText->GetFont();
	Font.Size = 24;
	MessageText->SetFont(Font);

	UCanvasPanelSlot* BannerSlot = RootCanvas->AddChildToCanvas(ConnectionFailureBanner);
	BannerSlot->SetAnchors(FAnchors(0.5f, 0.0f));
	BannerSlot->SetAlignment(FVector2D(0.5f, 0.0f));
	BannerSlot->SetPosition(FVector2D(0.0f, 60.0f));
	BannerSlot->SetSize(FVector2D(720.0f, 80.0f));

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ConnectionFailureMessageTimer,
			this,
			&UNPMainMenuWidget::HideConnectionFailureMessage,
			6.0f,
			false);
	}
}

void UNPMainMenuWidget::HideConnectionFailureMessage()
{
	if (IsValid(ConnectionFailureBanner))
	{
		ConnectionFailureBanner->RemoveFromParent();
		ConnectionFailureBanner = nullptr;
	}
}
