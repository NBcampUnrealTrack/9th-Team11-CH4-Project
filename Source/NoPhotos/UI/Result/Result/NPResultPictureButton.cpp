#include "UI/Result/Result/NPResultPictureButton.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Core/Main/NPMainGameState.h"
#include "Core/Main/NPMainPlayerController.h"
#include "UI/Result/Pictures/NPResultPicturePreviewPopup.h"

void UNPResultPictureButton::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureLikeButton();

	if (IsValid(ShowImageButton))
	{
		ShowImageButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleShowImageButtonClicked);
		ShowImageButton->SetIsEnabled(PhotoId.IsValid());
	}
	if (IsValid(LikeButton))
	{
		LikeButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleLikeButtonClicked);
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
}

void UNPResultPictureButton::NativeDestruct()
{
	if (IsValid(ShowImageButton))
	{
		ShowImageButton->OnClicked.RemoveAll(this);
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

void UNPResultPictureButton::NativeTick(
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

void UNPResultPictureButton::InitializePhoto(
	const FGuid InPhotoId,
	const FString& InCapturedPlayerName)
{
	PhotoId = InPhotoId;
	CapturedPlayerName = InCapturedPlayerName;
	if (IsValid(ShowImageButton))
	{
		ShowImageButton->SetIsEnabled(PhotoId.IsValid());
	}
	RefreshLikeState();
}

void UNPResultPictureButton::HandleShowImageButtonClicked()
{
	OpenPreview();
}

void UNPResultPictureButton::HandleLikeButtonClicked()
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

void UNPResultPictureButton::HandlePhotoLikesChanged(const FGuid ChangedPhotoId)
{
	if (!ChangedPhotoId.IsValid() || ChangedPhotoId == PhotoId)
	{
		bLikeRequestPending = false;
		RefreshLikeState();
	}
}

void UNPResultPictureButton::HandlePhotoFullyLiked(
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

void UNPResultPictureButton::EnsureLikeButton()
{
	if (IsValid(LikeButton))
	{
		if (!IsValid(LikeCountText))
		{
			LikeCountText = Cast<UTextBlock>(LikeButton->GetContent());
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

	LikeButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("LikeButton"));
	LikeCountText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("LikeCountText"));
	LikeButton->AddChild(LikeCountText);
	RootPanel->AddChild(LikeButton);

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(LikeButton->Slot))
	{
		CanvasSlot->SetAnchors(FAnchors(0.5f, 1.0f));
		CanvasSlot->SetAlignment(FVector2D(0.5f, 1.0f));
		CanvasSlot->SetPosition(FVector2D(0.0f, -4.0f));
		CanvasSlot->SetSize(FVector2D(90.0f, 30.0f));
		CanvasSlot->SetZOrder(20);
	}
}

void UNPResultPictureButton::RefreshLikeState()
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

void UNPResultPictureButton::StartFullyLikedPulse()
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

void UNPResultPictureButton::OpenPreview() const
{
	if (!PhotoId.IsValid() || !IsValid(PreviewPopupWidgetClass))
	{
		return;
	}

	UNPResultPicturePreviewPopup* PreviewPopup =
		CreateWidget<UNPResultPicturePreviewPopup>(GetOwningPlayer(), PreviewPopupWidgetClass);
	if (!IsValid(PreviewPopup))
	{
		return;
	}

	PreviewPopup->AddToViewport(200);
	PreviewPopup->OpenForPhoto(PhotoId, CapturedPlayerName);
}

