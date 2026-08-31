#pragma once

#include "CoreMinimal.h"
#include "UI/NPUserWidget.h"
#include "NPRelicHoverInfoWidget.generated.h"

class UTextBlock;

UCLASS()
class NOPHOTOS_API UNPRelicHoverInfoWidget : public UNPUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Relic Hover Info")
	void SetRelicInfo(const FText& InRelicName, int32 InPrice);
	
	UFUNCTION(BlueprintCallable, Category="Relic Hover Info")
	void ResetRelicInfo();

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> RelicNameText;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> RelicScoreText;
};