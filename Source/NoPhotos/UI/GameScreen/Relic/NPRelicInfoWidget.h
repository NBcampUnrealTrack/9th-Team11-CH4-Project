#pragma once

#include "CoreMinimal.h"
#include "UI/NPUserWidget.h"
#include "NPRelicInfoWidget.generated.h"

class UTextBlock;
class UWidgetAnimation;
class ANPBaseRelic;

UCLASS()
class NOPHOTOS_API UNPRelicInfoWidget : public UNPUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Relic Info")
	void SetRelicInfo(ANPBaseRelic* Relic);
	UFUNCTION(BlueprintCallable, Category="Relic Info")
	void ResetRelicInfo();

	void PlayShowAnimation();

protected:
	virtual void OnPopRequested_Implementation() override;
	virtual void NativeDestruct() override;

	UPROPERTY(Transient, meta=(BindWidgetAnim))
	TObjectPtr<UWidgetAnimation> ShowAnimation;

	UPROPERTY(Transient, meta=(BindWidgetAnim))
	TObjectPtr<UWidgetAnimation> OutAnimation;

private:
	void RefreshRelicScore();
	void HandleRelicValueChanged(ANPBaseRelic* Relic);
	void UnbindObservedRelic();

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> RelicNameText;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> RelicScoreText;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> RelicDescriptionText;

	TWeakObjectPtr<ANPBaseRelic> ObservedRelic;
};
