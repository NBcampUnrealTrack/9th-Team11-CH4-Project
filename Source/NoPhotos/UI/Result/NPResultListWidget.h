#pragma once

#include "CoreMinimal.h"
#include "Core/Main/NPMainGameState.h"
#include "TimerManager.h"
#include "UI/NPUserWidget.h"
#include "NPResultListWidget.generated.h"

class UVerticalBox;
class ANPMainGameState;
class UNPPersonalResultWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNPResultEntryRevealed, int32, Rank);

UCLASS()
class NOPHOTOS_API UNPResultListWidget : public UNPUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Result")
	FNPResultEntryRevealed OnResultEntryRevealed;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	void RefreshResultList();
	void AddNextResultEntry();
	void RefreshPictureLists();

	UFUNCTION()
	void HandlePhotoEvidenceChanged();

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> RankList;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UNPPersonalResultWidget> PersonalResultWidgetClass;

	TArray<FNPPlayerRanking> PendingPlayerRankings;
	TArray<TObjectPtr<UNPPersonalResultWidget>> ResultEntryWidgets;

	UPROPERTY(Transient)
	TObjectPtr<ANPMainGameState> ObservedGameState;
	int32 NextRankingIndex = INDEX_NONE;
	FTimerHandle ResultEntryTimer;
};
