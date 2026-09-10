#include "UI/Result/Pictures/NPSelectPictureWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Core/Main/NPMainGameState.h"
#include "Core/Main/NPMainPlayerController.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Gameplay/Photo/NPPhotoCaptureComponent.h"
#include "Core/Main/NPMainGameState.h"
#include "Core/Main/NPMainPlayerController.h"
#include "UI/Result/Pictures/NPPictureList.h"
#include "UI/Result/Pictures/NPSelectedPictureListWidget.h"
#include "UI/Result/Pictures/NPShowPicture.h"

void UNPSelectPictureWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (IsValid(PictureListWidget))
	{
		PictureListWidget->OnPictureClicked.AddUniqueDynamic(
			this,
			&UNPSelectPictureWidget::HandlePictureClicked);
	}

	if (IsValid(SelectedPictureListWidget))
	{
		SelectedPictureListWidget->OnPictureClicked.AddUniqueDynamic(
			this,
			&UNPSelectPictureWidget::HandleSelectedPictureClicked);
		SelectedPictureListWidget->OnPictureRemoveRequested.AddUniqueDynamic(
			this,
			&UNPSelectPictureWidget::HandleSelectedPictureRemoveRequested);
	}

	if (IsValid(ShowPictureWidget))
	{
		ShowPictureWidget->OnSelectRequested.AddUniqueDynamic(
			this,
			&UNPSelectPictureWidget::HandleSelectRequested);
	}

	if (IsValid(NextButton))
	{
		NextButton->OnClicked.AddUniqueDynamic(
			this,
			&UNPSelectPictureWidget::HandleNextButtonClicked);
	}

	if (ANPMainPlayerController* Controller =
		Cast<ANPMainPlayerController>(GetOwningPlayer()))
	{
		PhotoCaptureComponent = Controller->GetPhotoCaptureComponent();
	}

	ObservedGameState = GetWorld()
		? GetWorld()->GetGameState<ANPMainGameState>()
		: nullptr;

	if (IsValid(ObservedGameState))
	{
		ObservedGameState->OnPhotoEvidenceChanged.AddUniqueDynamic(
			this,
			&UNPSelectPictureWidget::HandlePhotoEvidenceChanged);
	}

	ObservedMainGameState = GetWorld()
		? GetWorld()->GetGameState<ANPMainGameState>()
		: nullptr;

	if (IsValid(ObservedMainGameState))
	{
		ObservedMainGameState->OnPictureSelectionStateChanged.AddUniqueDynamic(
			this,
			&UNPSelectPictureWidget::HandlePictureSelectionStateChanged);
	}

	UpdateSelectedPictureCountText();
	UpdateNextPlayerText();
	RequestOwnedPictures();
}

void UNPSelectPictureWidget::NativeDestruct()
{
	if (IsValid(ObservedGameState))
	{
		ObservedGameState->OnPhotoEvidenceChanged.RemoveDynamic(
			this,
			&UNPSelectPictureWidget::HandlePhotoEvidenceChanged);
	}

	if (IsValid(ObservedMainGameState))
	{
		ObservedMainGameState->OnPictureSelectionStateChanged.RemoveDynamic(
			this,
			&UNPSelectPictureWidget::HandlePictureSelectionStateChanged);
	}

	Super::NativeDestruct();
}

void UNPSelectPictureWidget::InitializePictures(
	const TArray<UTexture2D*>& InPictures)
{
	if (!IsValid(PictureListWidget))
	{
		return;
	}

	PictureListWidget->ClearPictures();

	if (IsValid(SelectedPictureListWidget))
	{
		SelectedPictureListWidget->ClearSelectedPictures();
	}

	PictureTextures.Empty();
	PicturePhotoIds.Empty();
	CurrentPictureIndex = INDEX_NONE;

	for (UTexture2D* PictureTexture : InPictures)
	{
		if (!IsValid(PictureTexture))
		{
			continue;
		}

		PictureTextures.Add(PictureTexture);
		PicturePhotoIds.Add(FGuid());
		PictureListWidget->AddPicture(PictureTexture);
	}

	if (!PictureTextures.IsEmpty())
	{
		ShowPicture(0);
	}

	UpdateSelectedPictureCountText();
}

TArray<FGuid> UNPSelectPictureWidget::GetSelectedPhotoIds() const
{
	TArray<FGuid> SelectedPhotoIds;

	for (const int32 PictureIndex : GetSelectedPictureIndices())
	{
		if (PicturePhotoIds.IsValidIndex(PictureIndex)
			&& PicturePhotoIds[PictureIndex].IsValid())
		{
			SelectedPhotoIds.Add(PicturePhotoIds[PictureIndex]);
		}
	}

	return SelectedPhotoIds;
}

void UNPSelectPictureWidget::HandlePictureClicked(
	const int32 PictureIndex)
{
	ShowPicture(PictureIndex);
}

void UNPSelectPictureWidget::HandleSelectRequested(
	const int32 PictureIndex)
{
	if (!IsValid(PictureListWidget)
		|| !PictureTextures.IsValidIndex(PictureIndex))
	{
		return;
	}

	const bool bWasSelected =
		PictureListWidget->IsPictureSelected(PictureIndex);

	if (bWasSelected)
	{
		PictureListWidget->SetPictureSelected(PictureIndex, false);
		if (IsValid(SelectedPictureListWidget))
		{
			SelectedPictureListWidget->RemoveSelectedPicture(PictureIndex);
		}
		ShowPictureWidget->SetSelected(false);
		UpdateSelectedPictureCountText();
		return;
	}

	if (PictureListWidget->GetSelectedPictureCount()
		>= MaxSelectedPictureCount)
	{
		return;
	}

	PictureListWidget->SetPictureSelected(PictureIndex, true);
	if (IsValid(SelectedPictureListWidget))
	{
		SelectedPictureListWidget->AddSelectedPicture(
			PictureTextures[PictureIndex],
			PictureIndex);
	}
	ShowPictureWidget->SetSelected(true);
	UpdateSelectedPictureCountText();
}

void UNPSelectPictureWidget::HandleSelectedPictureClicked(
	const int32 PictureIndex)
{
	ShowPicture(PictureIndex);
}

void UNPSelectPictureWidget::HandleSelectedPictureRemoveRequested(
	const int32 PictureIndex)
{
	if (!IsValid(PictureListWidget)
		|| !PictureTextures.IsValidIndex(PictureIndex)
		|| !PictureListWidget->IsPictureSelected(PictureIndex))
	{
		return;
	}

	PictureListWidget->SetPictureSelected(PictureIndex, false);

	if (IsValid(SelectedPictureListWidget))
	{
		SelectedPictureListWidget->RemoveSelectedPicture(PictureIndex);
	}

	if (CurrentPictureIndex == PictureIndex && IsValid(ShowPictureWidget))
	{
		ShowPictureWidget->SetSelected(false);
	}

	UpdateSelectedPictureCountText();
}

void UNPSelectPictureWidget::HandleNextButtonClicked()
{
	if (bWaitingForOtherPlayers)
	{
		return;
	}

	ANPMainPlayerController* MainPlayerController =
		Cast<ANPMainPlayerController>(GetOwningPlayer());

	if (!IsValid(MainPlayerController))
	{
		return;
	}

	bWaitingForOtherPlayers = true;
	
	if (IsValid(NextButton))
	{
		NextButton->SetIsEnabled(false);
	}

	if (IsValid(SelectedPictureCountText))
	{
		SelectedPictureCountText->SetText(FText::FromString(TEXT("대기 중...")));
	}

	BP_OnSelectionConfirmed(GetSelectedPictureIndices());
	MainPlayerController->ServerConfirmPictureSelection(GetSelectedPhotoIds());
}

void UNPSelectPictureWidget::HandlePhotoEvidenceChanged()
{
	RequestOwnedPictures();
}

void UNPSelectPictureWidget::HandlePictureSelectionStateChanged()
{
	UpdateNextPlayerText();
}

void UNPSelectPictureWidget::RequestOwnedPictures()
{
	APlayerController* OwningPlayer = GetOwningPlayer();
	APlayerState* LocalPlayerState = IsValid(OwningPlayer)
		? OwningPlayer->PlayerState
		: nullptr;
	if (!IsValid(PhotoCaptureComponent) || !IsValid(PictureListWidget)
		|| !IsValid(ObservedGameState) || !IsValid(LocalPlayerState))
	{
		return;
	}

	for (const FNPReplicatedPhotoEvidence& Evidence : ObservedGameState->GetPhotoEvidence())
	{
		const FGuid& PhotoId = Evidence.PhotoId;
		if (Evidence.Photographer != LocalPlayerState)
		{
			continue;
		}
		if (!PhotoId.IsValid() || RequestedPhotoIds.Contains(PhotoId))
		{
			continue;
		}

		UTexture2D* Texture = PhotoCaptureComponent->FindLocalPhotoTexture(PhotoId);
		if (!IsValid(Texture))
		{
			continue;
		}

		RequestedPhotoIds.Add(PhotoId);
		PictureTextures.Add(Texture);
		PicturePhotoIds.Add(PhotoId);
		PictureListWidget->AddPicture(Texture);
	}

	if (CurrentPictureIndex == INDEX_NONE)
	{
		ShowPicture(0);
	}

	UpdateSelectedPictureCountText();
}

void UNPSelectPictureWidget::ShowPicture(
	const int32 PictureIndex)
{
	if (!PictureTextures.IsValidIndex(PictureIndex)	|| !IsValid(ShowPictureWidget))
	{
		return;
	}

	CurrentPictureIndex = PictureIndex;

	ShowPictureWidget->SetPicture(PictureTextures[PictureIndex],	PictureIndex);

	if (IsValid(PictureListWidget))
	{
		ShowPictureWidget->SetSelected(PictureListWidget->IsPictureSelected(PictureIndex));
	}
}

void UNPSelectPictureWidget::UpdateSelectedPictureCountText()
{
	if (!IsValid(SelectedPictureCountText)
		|| !IsValid(PictureListWidget))
	{
		return;
	}

	const int32 SelectedCount =	PictureListWidget->GetSelectedPictureCount();

	SelectedPictureCountText->SetText(
		FText::FromString(FString::Printf(TEXT("%d / %d"), SelectedCount, MaxSelectedPictureCount)));
}

void UNPSelectPictureWidget::UpdateNextPlayerText()
{
	if (!IsValid(IsNextPlayer))
	{
		return;
	}

	if (!IsValid(ObservedMainGameState))
	{
		ObservedMainGameState = GetWorld() ? GetWorld()->GetGameState<ANPMainGameState>() : nullptr;

		if (IsValid(ObservedMainGameState))
		{
			ObservedMainGameState->OnPictureSelectionStateChanged.AddUniqueDynamic(
				this,
				&UNPSelectPictureWidget::HandlePictureSelectionStateChanged);
		}
	}

	if (!IsValid(ObservedMainGameState))
	{
		return;
	}

	const int32 TotalPlayerCount = ObservedMainGameState->PlayerArray.Num();
	int32 CompletedPlayerCount = 0;

	for (APlayerState* PlayerState : ObservedMainGameState->PlayerArray)
	{
		if (ObservedMainGameState->IsPlayerPictureSelectionComplete(PlayerState))
		{
			++CompletedPlayerCount;
		}
	}

	IsNextPlayer->SetText(FText::FromString(FString::Printf(TEXT("%d/%d명 완료"), CompletedPlayerCount, TotalPlayerCount)));
}

TArray<int32> UNPSelectPictureWidget::GetSelectedPictureIndices() const
{
	TArray<int32> SelectedIndices;

	if (!IsValid(PictureListWidget))
	{
		return SelectedIndices;
	}

	for (int32 PictureIndex = 0; PictureIndex < PictureTextures.Num(); ++PictureIndex)
	{
		if (PictureListWidget->IsPictureSelected(PictureIndex))
		{
			SelectedIndices.Add(PictureIndex);
		}
	}

	return SelectedIndices;
}
