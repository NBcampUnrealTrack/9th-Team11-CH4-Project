#include "UI/Result/Pictures/NPResultPicturePreviewPopup.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Core/Main/NPMainGameState.h"
#include "Core/Main/NPMainPlayerController.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Gameplay/Photo/NPPhotoTransferComponent.h"
#include "InputCoreTypes.h"
#include "Brushes/SlateColorBrush.h"
#include "TimerManager.h"

void UNPResultPicturePreviewPopup::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	EnsureModalBackdrop();
	EnsureDownloadButton();
	EnsureLikeControls();

	if (IsValid(CloseButton))
	{
		CloseButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCloseClicked);
	}
	if (IsValid(BackdropButton))
	{
		BackdropButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleBackdropClicked);
	}
	if (IsValid(DownloadButton))
	{
		DownloadButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleDownloadClicked);
	}
	if (IsValid(LikeButton))
	{
		LikeButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleLikeClicked);
	}

	ANPMainPlayerController* PlayerController = Cast<ANPMainPlayerController>(GetOwningPlayer());
	TransferComponent = IsValid(PlayerController) ? PlayerController->GetPhotoTransferComponent() : nullptr;
	if (IsValid(TransferComponent))
	{
		TransferComponent->OnPhotoTextureReceived.AddUniqueDynamic(
			this, &ThisClass::HandlePhotoTextureReceived);
	}

	ObservedGameState = GetWorld() ? GetWorld()->GetGameState<ANPMainGameState>() : nullptr;
	if (IsValid(ObservedGameState))
	{
		ObservedGameState->OnPhotoLikesChanged.AddUniqueDynamic(
			this, &ThisClass::HandlePhotoLikesChanged);
		ObservedGameState->OnPhotoFullyLiked.AddUniqueDynamic(
			this, &ThisClass::HandlePhotoFullyLiked);
	}
	RefreshLikeState();
	SetKeyboardFocus();
}

void UNPResultPicturePreviewPopup::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DownloadTimeoutTimer);
	}
	if (IsValid(TransferComponent))
	{
		TransferComponent->OnPhotoTextureReceived.RemoveDynamic(
			this, &ThisClass::HandlePhotoTextureReceived);
	}
	if (IsValid(CloseButton))
	{
		CloseButton->OnClicked.RemoveAll(this);
	}
	if (IsValid(BackdropButton))
	{
		BackdropButton->OnClicked.RemoveAll(this);
	}
	if (IsValid(DownloadButton))
	{
		DownloadButton->OnClicked.RemoveAll(this);
	}
	if (IsValid(LikeButton))
	{
		LikeButton->OnClicked.RemoveAll(this);
	}
	if (IsValid(ObservedGameState))
	{
		ObservedGameState->OnPhotoLikesChanged.RemoveDynamic(
			this, &ThisClass::HandlePhotoLikesChanged);
		ObservedGameState->OnPhotoFullyLiked.RemoveDynamic(
			this, &ThisClass::HandlePhotoFullyLiked);
	}
	if (IsValid(LikeCountText))
	{
		LikeCountText->SetRenderScale(FullyLikedPulseBaseScale);
	}
	Super::NativeDestruct();
}

FReply UNPResultPicturePreviewPopup::NativeOnPreviewKeyDown(
	const FGeometry& InGeometry,
	const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		HandleCloseClicked();
		return FReply::Handled();
	}

	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void UNPResultPicturePreviewPopup::NativeTick(
	const FGeometry& MyGeometry,
	const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bFullyLikedPulseActive || !IsValid(LikeCountText))
	{
		return;
	}

	FullyLikedPulseElapsed += InDeltaTime;
	const float Alpha = FMath::Clamp(
		FullyLikedPulseElapsed / FullyLikedPulseDuration, 0.0f, 1.0f);
	const float ScaleMultiplier = FMath::Lerp(
		1.0f, FullyLikedPulseScale, FMath::Sin(Alpha * PI));
	LikeCountText->SetRenderScale(FullyLikedPulseBaseScale * ScaleMultiplier);

	if (Alpha >= 1.0f)
	{
		LikeCountText->SetRenderScale(FullyLikedPulseBaseScale);
		bFullyLikedPulseActive = false;
	}
}

void UNPResultPicturePreviewPopup::OpenForPhoto(
	const FGuid InPhotoId,
	const FString& InCapturedPlayerName)
{
	PhotoId = InPhotoId;
	bPhotoRequestPending = false;
	bLikeRequestPending = false;
	RefreshLikeState();
	SetKeyboardFocus();
	if (IsValid(PreviewImage))
	{
		PreviewImage->SetVisibility(ESlateVisibility::Hidden);
	}

	if (IsValid(CapturedPlayerText))
	{
		const FString DisplayName = InCapturedPlayerName.IsEmpty() ? TEXT("익명") : InCapturedPlayerName;
		CapturedPlayerText->SetText(FText::FromString(FString::Printf(TEXT("%s 님이 찍힌 사진"), *DisplayName)));
	}

	if (IsValid(TransferComponent))
	{
		if (UTexture2D* CachedTexture = TransferComponent->FindReceivedPhoto(PhotoId))
		{
			DisplayPhoto(CachedTexture);
			return;
		}
	}

	if (IsValid(DownloadButton))
	{
		DownloadButton->SetIsEnabled(false);
	}
	SetDownloadButtonText(FText::FromString(TEXT("사진 불러오는 중...")));
	if (PhotoId.IsValid() && IsValid(TransferComponent))
	{
		bPhotoRequestPending = true;
		TransferComponent->RequestPhoto(PhotoId, true);
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				DownloadTimeoutTimer,
				this,
				&ThisClass::HandleDownloadTimeout,
				DownloadTimeoutSeconds,
				false);
		}
	}
}

void UNPResultPicturePreviewPopup::HandleCloseClicked()
{
	RemoveFromParent();
}

void UNPResultPicturePreviewPopup::HandleBackdropClicked()
{
	HandleCloseClicked();
}

void UNPResultPicturePreviewPopup::HandleDownloadClicked()
{
	if (bPhotoRequestPending || !PhotoId.IsValid() || !IsValid(TransferComponent)
		|| !IsValid(TransferComponent->FindReceivedPhoto(PhotoId)))
	{
		return;
	}

	FString SavedPath;
	if (TransferComponent->SaveReceivedPhotoToDisk(PhotoId, SavedPath))
	{
		if (IsValid(DownloadButton))
		{
			DownloadButton->SetIsEnabled(false);
		}
		SetDownloadButtonText(FText::FromString(TEXT("저장 완료")));
	}
	else
	{
		SetDownloadButtonText(FText::FromString(TEXT("저장 실패")));
	}
}

void UNPResultPicturePreviewPopup::HandleLikeClicked()
{
	ANPMainPlayerController* PlayerController =
		Cast<ANPMainPlayerController>(GetOwningPlayer());
	if (bLikeRequestPending || !IsValid(PlayerController)
		|| !IsValid(ObservedGameState)
		|| !ObservedGameState->CanPlayerLikePhoto(PhotoId, PlayerController->PlayerState))
	{
		return;
	}

	bLikeRequestPending = true;
	LikeButton->SetIsEnabled(false);
	PlayerController->ServerLikeResultPhoto(PhotoId);
}

void UNPResultPicturePreviewPopup::HandlePhotoLikesChanged(const FGuid ChangedPhotoId)
{
	if (!ChangedPhotoId.IsValid() || ChangedPhotoId == PhotoId)
	{
		bLikeRequestPending = false;
		RefreshLikeState();
	}
}

void UNPResultPicturePreviewPopup::HandlePhotoFullyLiked(
	const FGuid FullyLikedPhotoId,
	const int32 LikeCount)
{
	if (FullyLikedPhotoId != PhotoId)
	{
		return;
	}

	bLikeRequestPending = false;
	if (IsValid(LikeCountText))
	{
		LikeCountText->SetText(FText::FromString(
			FString::Printf(TEXT("♡ %d"), LikeCount)));
	}
	StartFullyLikedPulse();
}

void UNPResultPicturePreviewPopup::HandlePhotoTextureReceived(
	const FGuid ReceivedPhotoId,
	UTexture2D* Texture)
{
	if (ReceivedPhotoId == PhotoId && IsValid(Texture))
	{
		DisplayPhoto(Texture);
	}
}

void UNPResultPicturePreviewPopup::EnsureDownloadButton()
{
	if (IsValid(DownloadButton))
	{
		if (!IsValid(DownloadButtonText))
		{
			DownloadButtonText = Cast<UTextBlock>(DownloadButton->GetContent());
		}
		return;
	}
	if (!WidgetTree)
	{
		return;
	}

	UPanelWidget* RootPanel = Cast<UPanelWidget>(WidgetTree->RootWidget);
	if (!IsValid(RootPanel))
	{
		return;
	}

	DownloadButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("DownloadButton"));
	DownloadButtonText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("DownloadButtonText"));
	DownloadButtonText->SetText(FText::FromString(TEXT("내려받기")));
	DownloadButton->AddChild(DownloadButtonText);
	RootPanel->AddChild(DownloadButton);

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(DownloadButton->Slot))
	{
		CanvasSlot->SetAnchors(FAnchors(0.5f, 1.0f));
		CanvasSlot->SetAlignment(FVector2D(0.5f, 1.0f));
		CanvasSlot->SetPosition(FVector2D(0.0f, -40.0f));
		CanvasSlot->SetSize(FVector2D(180.0f, 50.0f));
		CanvasSlot->SetZOrder(10);
	}
}

void UNPResultPicturePreviewPopup::EnsureLikeControls()
{
	if (IsValid(LikeButton) && IsValid(LikeCountText))
	{
		return;
	}
	if (!WidgetTree)
	{
		return;
	}

	UPanelWidget* RootPanel = Cast<UPanelWidget>(WidgetTree->RootWidget);
	if (!IsValid(RootPanel))
	{
		return;
	}

	if (!IsValid(LikeButton) && !IsValid(LikeCountText))
	{
		UHorizontalBox* LikeBox = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), TEXT("LikeBox"));
		LikeButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("LikeButton"));
		LikeButtonText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("LikeButtonText"));
		LikeButtonText->SetText(FText::FromString(TEXT("좋아요")));
		LikeButton->AddChild(LikeButtonText);
		LikeCountText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("LikeCountText"));
		LikeBox->AddChild(LikeButton);
		LikeBox->AddChild(LikeCountText);
		RootPanel->AddChild(LikeBox);

		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(LikeBox->Slot))
		{
			CanvasSlot->SetAnchors(FAnchors(0.5f, 1.0f));
			CanvasSlot->SetAlignment(FVector2D(0.5f, 1.0f));
			CanvasSlot->SetPosition(FVector2D(0.0f, -100.0f));
			CanvasSlot->SetSize(FVector2D(220.0f, 50.0f));
			CanvasSlot->SetZOrder(20);
		}
		return;
	}

	if (!IsValid(LikeButton))
	{
		LikeButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("LikeButton"));
		LikeButtonText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("LikeButtonText"));
		LikeButtonText->SetText(FText::FromString(TEXT("좋아요")));
		LikeButton->AddChild(LikeButtonText);
		RootPanel->AddChild(LikeButton);
	}
	if (!IsValid(LikeCountText))
	{
		LikeCountText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("LikeCountText"));
		RootPanel->AddChild(LikeCountText);
	}
}

void UNPResultPicturePreviewPopup::EnsureModalBackdrop()
{
	if (IsValid(BackdropButton))
	{
		return;
	}
	if (!WidgetTree)
	{
		return;
	}

	UPanelWidget* RootPanel = Cast<UPanelWidget>(WidgetTree->RootWidget);
	if (!IsValid(RootPanel))
	{
		return;
	}

	BackdropButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("BackdropButton"));
	const FSlateColorBrush DimBrush(
		FLinearColor(0.0f, 0.0f, 0.0f, BackdropOpacity));
	FButtonStyle BackdropStyle;
	BackdropStyle.SetNormal(DimBrush);
	BackdropStyle.SetHovered(DimBrush);
	BackdropStyle.SetPressed(DimBrush);
	BackdropButton->SetStyle(BackdropStyle);
	RootPanel->AddChild(BackdropButton);

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(BackdropButton->Slot))
	{
		CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		CanvasSlot->SetOffsets(FMargin(0.0f));
		CanvasSlot->SetZOrder(-100);
	}
}

void UNPResultPicturePreviewPopup::RefreshLikeState()
{
	if (!IsValid(LikeButton) || !IsValid(LikeCountText))
	{
		return;
	}

	const int32 LikeCount = IsValid(ObservedGameState)
		? ObservedGameState->GetPhotoLikeCount(PhotoId)
		: 0;
	LikeCountText->SetText(FText::FromString(
		FString::Printf(TEXT("♡ %d"), LikeCount)));

	const ANPMainPlayerController* PlayerController =
		Cast<ANPMainPlayerController>(GetOwningPlayer());
	const bool bCanLike = IsValid(PlayerController)
		&& IsValid(ObservedGameState)
		&& ObservedGameState->CanPlayerLikePhoto(PhotoId, PlayerController->PlayerState);
	LikeButton->SetIsEnabled(bCanLike && !bLikeRequestPending);
}

void UNPResultPicturePreviewPopup::StartFullyLikedPulse()
{
	if (!IsValid(LikeCountText))
	{
		return;
	}

	FullyLikedPulseBaseScale = LikeCountText->GetRenderTransform().Scale;
	LikeCountText->SetRenderTransformPivot(FVector2D(0.5f));
	FullyLikedPulseElapsed = 0.0f;
	bFullyLikedPulseActive = true;
}

void UNPResultPicturePreviewPopup::DisplayPhoto(UTexture2D* Texture)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DownloadTimeoutTimer);
	}
	if (IsValid(PreviewImage) && IsValid(Texture))
	{
		PreviewImage->SetBrushFromTexture(Texture);
		PreviewImage->SetVisibility(ESlateVisibility::Visible);
	}
	bPhotoRequestPending = false;
	if (IsValid(DownloadButton))
	{
		DownloadButton->SetIsEnabled(true);
	}
	SetDownloadButtonText(FText::FromString(TEXT("내려받기")));
}

void UNPResultPicturePreviewPopup::HandleDownloadTimeout()
{
	bPhotoRequestPending = false;
	if (IsValid(DownloadButton))
	{
		DownloadButton->SetIsEnabled(false);
	}
	SetDownloadButtonText(FText::FromString(TEXT("사진 불러오기 실패")));
}

void UNPResultPicturePreviewPopup::SetDownloadButtonText(const FText& Text) const
{
	if (IsValid(DownloadButtonText))
	{
		DownloadButtonText->SetText(Text);
	}
}
