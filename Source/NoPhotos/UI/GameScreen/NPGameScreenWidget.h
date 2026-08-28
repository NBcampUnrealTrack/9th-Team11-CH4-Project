#pragma once

#include "CoreMinimal.h"
#include "UI/NPUserWidget.h"
#include "NPGameScreenWidget.generated.h"

class UNPEventTimerBarWidget;
class UNPMapEventManagerComponent;

UCLASS()
class NOPHOTOS_API UNPGameScreenWidget : public UNPUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UNPEventTimerBarWidget> EventTimerBar;

	FTimerHandle EventManagerBindRetryTimerHandle;
	TWeakObjectPtr<UNPMapEventManagerComponent> BoundEventManager;

	UFUNCTION()
	void HandleActiveMapEventsChanged();

	bool TryBindToEventManager();
	void RetryBindToEventManager();
	void UnbindFromEventManager();
	void RefreshEventTimer();
	void SetEventTimerVisible(bool bVisible);
};