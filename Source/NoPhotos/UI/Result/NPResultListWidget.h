#pragma once

#include "CoreMinimal.h"
#include "Core/Main/NPMainGameState.h"
#include "TimerManager.h"
#include "UI/NPUserWidget.h"
#include "NPResultListWidget.generated.h"

class UVerticalBox;
class ANPMainGameState;
class UNPPhotoTransferComponent;
class UNPPersonalResultWidget;
class UTexture2D;

UCLASS()
class NOPHOTOS_API UNPResultListWidget : public UNPUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	void RefreshResultList();
	void AddNextResultEntry();
	void RefreshPictureLists();
	void RebuildPhotoDownloadQueue();
	void RequestNextPhoto();

	UFUNCTION()
	void HandlePhotoEvidenceChanged();
	UFUNCTION()
	void HandlePhotoTextureReceived(FGuid PhotoId, UTexture2D* Texture);

private:
	struct FQueuedPhotoDownload
	{
		TWeakObjectPtr<UNPPersonalResultWidget> TargetWidget;
		FGuid PhotoId;
	};

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> RankList;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UNPPersonalResultWidget> PersonalResultWidgetClass;

	TArray<FNPPlayerRanking> PendingPlayerRankings;
	TArray<TObjectPtr<UNPPersonalResultWidget>> ResultEntryWidgets;
	TArray<FQueuedPhotoDownload> PendingPhotoDownloads;
	TWeakObjectPtr<UNPPersonalResultWidget> DownloadTargetWidget;
	FGuid DownloadingPhotoId;
	bool bPhotoQueueRefreshPending = false;

	UPROPERTY(Transient)
	TObjectPtr<ANPMainGameState> ObservedGameState;
	UPROPERTY(Transient)
	TObjectPtr<UNPPhotoTransferComponent> TransferComponent;
	int32 NextRankingIndex = INDEX_NONE;
	FTimerHandle ResultEntryTimer;
};
