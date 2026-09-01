#include "UI/Result/NPPersonalResultWidget.h"

#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Core/Main/NPMainGameState.h"
#include "Core/Main/NPMainPlayerController.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Gameplay/Photo/NPPhotoTransferComponent.h"
#include "UI/Result/Result/NPResultPictureButton.h"

void UNPPersonalResultWidget::NativeDestruct()
{
	if (IsValid(TransferComponent))
	{
		TransferComponent->OnPhotoTextureReceived.RemoveDynamic(this, &ThisClass::HandlePhotoTextureReceived);
	}

	Super::NativeDestruct();
}

void UNPPersonalResultWidget::SetupResult(const int32 InRank, const FString& InPlayerName, const int32 InScore, APlayerState* InPlayerState)
{
	ResultPlayerState = InPlayerState;

	if (IsValid(RankText))
	{
		RankText->SetText(FText::AsNumber(InRank));
	}

	if (IsValid(PlayerNameText))
	{
		PlayerNameText->SetText(FText::FromString(InPlayerName));
	}

	if (IsValid(ScoreText))
	{
		ScoreText->SetText(FText::AsNumber(InScore));
	}

	CreatePictureButtons();
}

void UNPPersonalResultWidget::CreatePictureButtons()
{
	if (!IsValid(PictureList))
	{
		return;
	}

	PictureList->ClearChildren();
	if (IsValid(TransferComponent))
	{
		TransferComponent->OnPhotoTextureReceived.RemoveDynamic(this, &ThisClass::HandlePhotoTextureReceived);
	}

	PictureButtonsById.Empty();
	PendingPhotoIds.Empty();
	DownloadingPhotoId.Invalidate();
	if (!IsValid(ResultPlayerState) || !IsValid(PictureButtonWidgetClass))
	{
		return;
	}

	ANPMainGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ANPMainGameState>() : nullptr;
	if (!IsValid(GameState))
	{
		return;
	}

	PendingPhotoIds = GameState->GetSelectedPhotoIds(ResultPlayerState);
	for (const FGuid& PhotoId : PendingPhotoIds)
	{
		if (!PhotoId.IsValid())
		{
			continue;
		}

		UNPResultPictureButton* PictureButton =
			CreateWidget<UNPResultPictureButton>(GetOwningPlayer(), PictureButtonWidgetClass);
		if (!IsValid(PictureButton))
		{
			continue;
		}

		PictureList->AddChild(PictureButton);
		PictureButtonsById.Add(PhotoId, PictureButton);
	}

	ANPMainPlayerController* PlayerController = Cast<ANPMainPlayerController>(GetOwningPlayer());
	TransferComponent = IsValid(PlayerController) ? PlayerController->GetPhotoTransferComponent() : nullptr;
	if (!IsValid(TransferComponent))
	{
		return;
	}

	TransferComponent->OnPhotoTextureReceived.AddUniqueDynamic(this, &ThisClass::HandlePhotoTextureReceived);
	RequestNextPhoto();
}

void UNPPersonalResultWidget::RequestNextPhoto()
{
	if (!IsValid(TransferComponent) || DownloadingPhotoId.IsValid())
	{
		return;
	}

	while (!PendingPhotoIds.IsEmpty())
	{
		const FGuid NextPhotoId = PendingPhotoIds[0];
		PendingPhotoIds.RemoveAt(0);
		if (!NextPhotoId.IsValid() || !PictureButtonsById.Contains(NextPhotoId))
		{
			continue;
		}

		DownloadingPhotoId = NextPhotoId;
		if (UTexture2D* CachedTexture = TransferComponent->FindReceivedPhoto(NextPhotoId))
		{
			HandlePhotoTextureReceived(NextPhotoId, CachedTexture);
			return;
		}

		TransferComponent->RequestPhoto(NextPhotoId);
		return;
	}
}

void UNPPersonalResultWidget::HandlePhotoTextureReceived(const FGuid PhotoId, UTexture2D* Texture)
{
	if (PhotoId != DownloadingPhotoId || !IsValid(Texture))
	{
		return;
	}

	DownloadingPhotoId.Invalidate();
	if (TObjectPtr<UNPResultPictureButton>* PictureButton = PictureButtonsById.Find(PhotoId))
	{
		if (IsValid(*PictureButton))
		{
			FString CapturedPlayerName;
			if (ANPMainGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ANPMainGameState>() : nullptr)
			{
				for (const FNPReplicatedPhotoEvidence& Evidence : GameState->GetPhotoEvidence())
				{
					if (Evidence.PhotoId == PhotoId && IsValid(Evidence.Thief))
					{
						CapturedPlayerName = Evidence.Thief->GetPlayerName();
						break;
					}
				}
			}

			(*PictureButton)->SetPictureTexture(Texture, CapturedPlayerName);
		}
	}

	RequestNextPhoto();
}
