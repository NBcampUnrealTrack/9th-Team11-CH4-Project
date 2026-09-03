#pragma once

#include "CoreMinimal.h"
#include "UI/NPUserWidget.h"
#include "NPPhotoPenaltyWidget.generated.h"

class UTextBlock;

/** 유물 증거 사진으로 실제 감소한 가격을 표시하는 월드 공간 위젯입니다. */
UCLASS()
class NOPHOTOS_API UNPPhotoPenaltyWidget : public UNPUserWidget
{
	GENERATED_BODY()

public:
	void SetPenaltyAmount(int32 AppliedPhotoPenalty);

private:
	/** 파생 WBP에서 같은 이름의 TextBlock을 만들어야 합니다. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> PhotoPenaltyText;
};
