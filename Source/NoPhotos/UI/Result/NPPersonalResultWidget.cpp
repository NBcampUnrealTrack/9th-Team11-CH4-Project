#include "UI/Result/NPPersonalResultWidget.h"

#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Core/Main/NPMainGameState.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "UI/Result/Result/NPResultPictureButton.h"

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

void UNPPersonalResultWidget::RefreshPictureButtons()
{
	CreatePictureButtons();
}

void UNPPersonalResultWidget::CreatePictureButtons()
{
	if (!IsValid(PictureList))
	{
		return;
	}

	PictureList->ClearChildren();
	if (!IsValid(ResultPlayerState) || !IsValid(PictureButtonWidgetClass))
	{
		return;
	}

	ANPMainGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ANPMainGameState>() : nullptr;
	if (!IsValid(GameState))
	{
		return;
	}

	for (const FGuid& PhotoId : GameState->GetSelectedPhotoIds(ResultPlayerState))
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

		FString CapturedPlayerName;
		for (const FNPReplicatedPhotoEvidence& Evidence : GameState->GetPhotoEvidence())
		{
			if (Evidence.PhotoId == PhotoId && IsValid(Evidence.Thief))
			{
				CapturedPlayerName = Evidence.Thief->GetPlayerName();
				break;
			}
		}
		PictureButton->InitializePhoto(PhotoId, CapturedPlayerName);
		if (UHorizontalBoxSlot* PictureSlot = Cast<UHorizontalBoxSlot>(PictureList->AddChild(PictureButton)))
		{
			FMargin SlotPadding = PictureSlot->GetPadding();
			SlotPadding.Left = -5.0f;
			PictureSlot->SetPadding(SlotPadding);
		}
	}
}
