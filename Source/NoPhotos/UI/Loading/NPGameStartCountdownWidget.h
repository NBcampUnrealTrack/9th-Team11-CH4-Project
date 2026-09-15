#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NPGameStartCountdownWidget.generated.h"

class UTextBlock;

/** 서버 시각에 동기화된 3, 2, 1, 게임 시작 표시 위젯입니다. */
UCLASS(Blueprintable)
class NOPHOTOS_API UNPGameStartCountdownWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Step은 3, 2, 1이며 0은 게임 시작 문구를 의미합니다. */
	UFUNCTION(BlueprintCallable, Category="Main Game|Countdown")
	void SetCountdownStep(int32 Step);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	UFUNCTION(BlueprintImplementableEvent, Category="Main Game|Countdown",
		meta=(DisplayName="On Countdown Step Changed"))
	void BP_OnCountdownStepChanged(int32 Step, const FText& DisplayText);

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> CountdownText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Main Game|Countdown")
	FText GameStartText = NSLOCTEXT(
		"NoPhotos",
		"GameStartCountdownGo",
		"게임 시작!");

private:
	int32 CurrentStep = INDEX_NONE;
};
