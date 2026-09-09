#pragma once

#include "CoreMinimal.h"
#include "UI/Loading/NPMainWorldLoadingWidget.h"
#include "NPLoadingWidget.generated.h"

UCLASS()
class NOPHOTOS_API UNPLoadingWidget : public UNPMainWorldLoadingWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Loading")
	void UpdateLoadingProgress();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<class UProgressBar> LoadingProgressBar;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<class UTextBlock> LoadingPercentText;
};
