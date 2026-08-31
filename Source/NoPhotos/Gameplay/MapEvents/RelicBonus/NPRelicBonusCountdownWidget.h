#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NPRelicBonusCountdownWidget.generated.h"

class UTextBlock;

/** 별도 WBP 없이 사용할 수 있는 유물 보너스 월드 카운트다운 위젯입니다. */
UCLASS()
class NOPHOTOS_API UNPRelicBonusCountdownWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetRemainingSeconds(int32 RemainingSeconds);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	void BuildDefaultWidget();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RemainingTimeText;

	int32 CachedRemainingSeconds = INDEX_NONE;
};
