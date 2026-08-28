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

	/** 유물 정보 UI가 나타날 때 ShowAnimation을 정방향으로 재생합니다. */
	void PlayShowAnimation();

protected:
	virtual void OnPopRequested_Implementation() override;

	/** WBP_RelicInfoWidget의 ShowAnimation을 연결합니다. */
	UPROPERTY(Transient, meta=(BindWidgetAnim))
	TObjectPtr<UWidgetAnimation> ShowAnimation;

	/** WBP_RelicInfoWidget의 OutAnimation을 연결합니다. */
	UPROPERTY(Transient, meta=(BindWidgetAnim))
	TObjectPtr<UWidgetAnimation> OutAnimation;

private:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> RelicNameText;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> RelicScoreText;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> RelicDescriptionText;
};
