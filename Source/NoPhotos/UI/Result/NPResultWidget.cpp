#include "UI/Result/NPResultWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Widget.h"
#include "Core/Audio/NPSoundSubsystem.h"
#include "Core/Main/NPMainGameState.h"
#include "Core/Main/NPMainPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UI/Result/NPResultListWidget.h"

UNPResultWidget::UNPResultWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UNPResultWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ANPMainPlayerController* NPPC = Cast<ANPMainPlayerController>(GetOwningPlayer());
	const bool bIsRoomHost = IsValid(NPPC) && NPPC->IsListenServerHost();

	if (IsValid(RetryButton))
	{
		RetryButton->SetVisibility(
			bIsRoomHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

		if (bIsRoomHost)
		{
			RetryButton->OnClicked.AddDynamic(this, &UNPResultWidget::OnRetryClicked);
		}
	}

	if (IsValid(ExitButton))
	{
		ExitButton->OnClicked.AddDynamic(this, &UNPResultWidget::OnExitClicked);
	}

	BindResultListWidget();

	ObservedGameState = GetWorld() ? GetWorld()->GetGameState<ANPMainGameState>() : nullptr;
	if (IsValid(ObservedGameState))
	{
		ObservedGameState->OnPhotoFullyLiked.AddUniqueDynamic(
			this, &ThisClass::HandlePhotoFullyLiked);
	}
}

void UNPResultWidget::NativeDestruct()
{
	if (IsValid(RetryButton))
	{
		RetryButton->OnClicked.RemoveAll(this);
	}

	if (IsValid(ExitButton))
	{
		ExitButton->OnClicked.RemoveAll(this);
	}
	if (IsValid(ObservedGameState))
	{
		ObservedGameState->OnPhotoFullyLiked.RemoveDynamic(
			this, &ThisClass::HandlePhotoFullyLiked);
	}
	if (IsValid(ResultListWidget))
	{
		ResultListWidget->OnResultEntryRevealed.RemoveDynamic(
			this, &ThisClass::HandleResultEntryRevealed);
	}

	Super::NativeDestruct();
}

void UNPResultWidget::OnRetryClicked()
{
	ANPMainPlayerController* NPPC = Cast<ANPMainPlayerController>(GetOwningPlayer());
	if (!IsValid(NPPC) || !NPPC->IsListenServerHost())
	{
		return;
	}

	NPPC->RequestRestartRoom();
}

void UNPResultWidget::OnExitClicked()
{
	if (ANPMainPlayerController* NPPC=Cast<ANPMainPlayerController>(GetOwningPlayer()))
	{
		NPPC->ExitToMainMenu();
	}
}

void UNPResultWidget::HandlePhotoFullyLiked(
	const FGuid,
	const int32)
{
	if (IsValid(FullyLikedSound))
	{
		UGameplayStatics::PlaySound2D(this, FullyLikedSound);
	}
}

void UNPResultWidget::HandleResultEntryRevealed(const int32 Rank)
{
	const int32 SoundIndex = Rank == 1 ? 0 : 1;
	if (RankingRevealSounds.IsValidIndex(SoundIndex)
		&& IsValid(RankingRevealSounds[SoundIndex]))
	{
		if (UNPSoundSubsystem* SoundSubsystem = UNPSoundSubsystem::Get(this))
		{
			SoundSubsystem->PlaySFX(RankingRevealSounds[SoundIndex]);
		}
	}
}

void UNPResultWidget::BindResultListWidget()
{
	if (!IsValid(ResultListWidget) && IsValid(WidgetTree))
	{
		WidgetTree->ForEachWidget([this](UWidget* Widget)
		{
			if (!IsValid(ResultListWidget))
			{
				ResultListWidget = Cast<UNPResultListWidget>(Widget);
			}
		});
	}

	if (IsValid(ResultListWidget))
	{
		ResultListWidget->OnResultEntryRevealed.AddUniqueDynamic(
			this, &ThisClass::HandleResultEntryRevealed);
	}
}
