#pragma once

#include "CoreMinimal.h"
#include "Core/Main/NPMainGameState.h"
#include "TimerManager.h"
#include "UI/NPUserWidget.h"
#include "NPResultListWidget.generated.h"

class UVerticalBox;
class UNPPersonalResultWidget;

UCLASS()
class NOPHOTOS_API UNPResultListWidget : public UNPUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	void RefreshResultList();
	void AddNextResultEntry();

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> RankList;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UNPPersonalResultWidget> PersonalResultWidgetClass;

	TArray<FNPPlayerRanking> PendingPlayerRankings;
	TArray<TObjectPtr<UNPPersonalResultWidget>> ResultEntryWidgets;
	int32 NextRankingIndex = INDEX_NONE;
	FTimerHandle ResultEntryTimer;
};
