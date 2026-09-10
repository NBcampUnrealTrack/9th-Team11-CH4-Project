#include "UI/Result/NPResultListWidget.h"

#include "Components/VerticalBox.h"
#include "Core/Main/NPMainGameState.h"
#include "Core/NPPlayerState.h"
#include "Engine/World.h"
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
	PendingPlayerRankings.Reset();
	ResultEntryWidgets.Reset();
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
