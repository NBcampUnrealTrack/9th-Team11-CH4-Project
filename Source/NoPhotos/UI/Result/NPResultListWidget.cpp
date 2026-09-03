#include "UI/Result/NPResultListWidget.h"

#include "Components/VerticalBox.h"
#include "Core/Main/NPMainGameState.h"
#include "Core/Main/NPMainPlayerController.h"
#include "Core/NPPlayerState.h"
#include "Engine/World.h"
#include "Gameplay/Photo/NPPhotoTransferComponent.h"
#include "TimerManager.h"
#include "UI/Result/NPPersonalResultWidget.h"

void UNPResultListWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RefreshResultList();
	ObservedGameState = GetWorld() ? GetWorld()->GetGameState<ANPMainGameState>() : nullptr;
	if (IsValid(ObservedGameState))
	{
		ObservedGameState->OnPhotoEvidenceChanged.AddUniqueDynamic(
			this, &ThisClass::HandlePhotoEvidenceChanged);
	}

	ANPMainPlayerController* PlayerController = Cast<ANPMainPlayerController>(GetOwningPlayer());
	TransferComponent = IsValid(PlayerController) ? PlayerController->GetPhotoTransferComponent() : nullptr;
	if (IsValid(TransferComponent))
	{
		TransferComponent->OnPhotoTextureReceived.AddUniqueDynamic(
			this, &ThisClass::HandlePhotoTextureReceived);
	}

	RebuildPhotoDownloadQueue();
}

void UNPResultListWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ResultEntryTimer);
	}
	if (IsValid(ObservedGameState))
	{
		ObservedGameState->OnPhotoEvidenceChanged.RemoveDynamic(
			this, &ThisClass::HandlePhotoEvidenceChanged);
	}
	if (IsValid(TransferComponent))
	{
		TransferComponent->OnPhotoTextureReceived.RemoveDynamic(
			this, &ThisClass::HandlePhotoTextureReceived);
	}

	PendingPlayerRankings.Reset();
	ResultEntryWidgets.Reset();
	PendingPhotoDownloads.Reset();
	DownloadingPhotoId.Invalidate();
	DownloadTargetWidget.Reset();
	NextRankingIndex = INDEX_NONE;

	Super::NativeDestruct();
}

void UNPResultListWidget::RefreshResultList()
{
	if (!IsValid(RankList) || !IsValid(PersonalResultWidgetClass))
	{
		return;
	}

	ANPMainGameState* NPMGS = GetWorld() ? GetWorld()->GetGameState<ANPMainGameState>()	: nullptr;

	if (!IsValid(NPMGS))
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ResultEntryTimer);
	}

	RankList->ClearChildren();
	PendingPlayerRankings = NPMGS->GetPlayerRankings();
	ResultEntryWidgets.Reset();
	ResultEntryWidgets.Reserve(PendingPlayerRankings.Num());

	// 최종 순서(1위 -> 최하위)를 먼저 만들고 모두 숨긴다.
	// Hidden은 레이아웃 공간을 유지하므로 최하위가 처음부터 제 자리에 나타난다.
	for (int32 RankingIndex = 0;
		RankingIndex < PendingPlayerRankings.Num();
		++RankingIndex)
	{
		const FNPPlayerRanking& Ranking =
			PendingPlayerRankings[RankingIndex];
		const FString PlayerName = IsValid(Ranking.PlayerState)
			? Ranking.PlayerState->GetPlayerName()
			: TEXT("Unknown");

		UNPPersonalResultWidget* PersonalResultWidget =
			CreateWidget<UNPPersonalResultWidget>(
				GetOwningPlayer(),
				PersonalResultWidgetClass);

		if (!IsValid(PersonalResultWidget))
		{
			ResultEntryWidgets.Add(nullptr);
			continue;
		}

		PersonalResultWidget->SetupResult(
			RankingIndex + 1,
			PlayerName,
			Ranking.Score,
			Ranking.PlayerState);
		PersonalResultWidget->SetVisibility(ESlateVisibility::Hidden);
		RankList->AddChild(PersonalResultWidget);
		ResultEntryWidgets.Add(PersonalResultWidget);
	}

	NextRankingIndex = ResultEntryWidgets.Num() - 1;

	if (NextRankingIndex >= 0)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				ResultEntryTimer,
				this,
				&ThisClass::AddNextResultEntry,
				0.5f,
				false);
		}
	}
}

void UNPResultListWidget::HandlePhotoEvidenceChanged()
{
	RefreshPictureLists();
}

void UNPResultListWidget::RefreshPictureLists()
{
	for (UNPPersonalResultWidget* ResultEntryWidget : ResultEntryWidgets)
	{
		if (IsValid(ResultEntryWidget))
		{
			ResultEntryWidget->RefreshPictureButtons();
		}
	}

	if (DownloadingPhotoId.IsValid())
	{
		bPhotoQueueRefreshPending = true;
		return;
	}

	RebuildPhotoDownloadQueue();
}

void UNPResultListWidget::RebuildPhotoDownloadQueue()
{
	if (!IsValid(TransferComponent) || DownloadingPhotoId.IsValid())
	{
		return;
	}

	bPhotoQueueRefreshPending = false;
	PendingPhotoDownloads.Reset();
	for (UNPPersonalResultWidget* ResultEntryWidget : ResultEntryWidgets)
	{
		if (!IsValid(ResultEntryWidget))
		{
			continue;
		}

		for (const FGuid& PhotoId : ResultEntryWidget->GetPicturePhotoIds())
		{
			if (!PhotoId.IsValid())
			{
				continue;
			}

			if (UTexture2D* CachedTexture = TransferComponent->FindReceivedPhoto(PhotoId))
			{
				ResultEntryWidget->SetPictureTexture(PhotoId, CachedTexture);
				continue;
			}

			PendingPhotoDownloads.Add({ResultEntryWidget, PhotoId});
		}
	}

	RequestNextPhoto();
}

void UNPResultListWidget::RequestNextPhoto()
{
	if (!IsValid(TransferComponent) || DownloadingPhotoId.IsValid())
	{
		return;
	}

	while (!PendingPhotoDownloads.IsEmpty())
	{
		const FQueuedPhotoDownload NextDownload = PendingPhotoDownloads[0];
		PendingPhotoDownloads.RemoveAt(0);
		if (!NextDownload.PhotoId.IsValid() || !NextDownload.TargetWidget.IsValid())
		{
			continue;
		}

		DownloadingPhotoId = NextDownload.PhotoId;
		DownloadTargetWidget = NextDownload.TargetWidget;
		TransferComponent->RequestPhoto(DownloadingPhotoId);
		return;
	}
}

void UNPResultListWidget::HandlePhotoTextureReceived(const FGuid PhotoId, UTexture2D* Texture)
{
	if (PhotoId != DownloadingPhotoId || !IsValid(Texture))
	{
		return;
	}

	if (DownloadTargetWidget.IsValid())
	{
		DownloadTargetWidget->SetPictureTexture(PhotoId, Texture);
	}
	DownloadingPhotoId.Invalidate();
	DownloadTargetWidget.Reset();

	if (bPhotoQueueRefreshPending)
	{
		RebuildPhotoDownloadQueue();
		return;
	}

	RequestNextPhoto();
}

void UNPResultListWidget::AddNextResultEntry()
{
	if (!ResultEntryWidgets.IsValidIndex(NextRankingIndex))
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(ResultEntryTimer);
		}
		return;
	}

	if (UNPPersonalResultWidget* PersonalResultWidget =
		ResultEntryWidgets[NextRankingIndex])
	{
		PersonalResultWidget->SetVisibility(ESlateVisibility::Visible);
	}

	--NextRankingIndex;

	if (NextRankingIndex < 0)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(ResultEntryTimer);
		}
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ResultEntryTimer,
			this,
			&ThisClass::AddNextResultEntry,
			0.5f,
			false);
	}
}
