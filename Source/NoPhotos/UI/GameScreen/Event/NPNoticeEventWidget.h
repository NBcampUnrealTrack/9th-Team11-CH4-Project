#pragma once

#include "CoreMinimal.h"
#include "UI/NPUserWidget.h"
#include "NPNoticeEventWidget.generated.h"

class UTextBlock;
class UWidgetAnimation;
class UNPMapEventManagerComponent;

UCLASS()
class NOPHOTOS_API UNPNoticeEventWidget : public UNPUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void OnAnimationFinished_Implementation(const UWidgetAnimation* Animation) override;

private:
	void ShowEventNotice(const FText& EventTitle);
	void HideEventNotice();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> EventNameText;

	UPROPERTY(Transient, meta = (BindWidgetAnim))
	TObjectPtr<UWidgetAnimation> NoticeAnimation;

	FTimerHandle EventManagerBindRetryTimerHandle;
	TWeakObjectPtr<UNPMapEventManagerComponent> BoundEventManager;
	TSet<FName> KnownActiveEventIds;

	UFUNCTION()
	void HandleActiveMapEventsChanged();

	bool TryBindToEventManager();
	void RetryBindToEventManager();
	void UnbindFromEventManager();
	void RefreshActiveEventNotices(bool bShowNewNotices);
};
