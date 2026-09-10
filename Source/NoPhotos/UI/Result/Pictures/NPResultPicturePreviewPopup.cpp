#include "UI/Result/Pictures/NPResultPicturePreviewPopup.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Core/Main/NPMainPlayerController.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Gameplay/Photo/NPPhotoTransferComponent.h"
#include "TimerManager.h"

void UNPResultPicturePreviewPopup::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureDownloadButton();

	if (IsValid(CloseButton))
	{
		CloseButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCloseClicked);
	}
	if (IsValid(DownloadButton))
	{
		DownloadButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleDownloadClicked);
	}

	ANPMainPlayerController* PlayerController = Cast<ANPMainPlayerController>(GetOwningPlayer());
	TransferComponent = IsValid(PlayerController) ? PlayerController->GetPhotoTransferComponent() : nullptr;
	if (IsValid(TransferComponent))
	{
		TransferComponent->OnPhotoTextureReceived.AddUniqueDynamic(
			this, &ThisClass::HandlePhotoTextureReceived);
	}
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
	Super::NativeDestruct();
}

void UNPResultPicturePreviewPopup::OpenForPhoto(
	const FGuid InPhotoId,
	const FString& InCapturedPlayerName)
{
	PhotoId = InPhotoId;
	bPhotoRequestPending = false;
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
