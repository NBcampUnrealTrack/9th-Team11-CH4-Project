#pragma once

#include "CoreMinimal.h"
#include "UI/NPUserWidget.h"
#include "NPScoreFeedbackWidget.generated.h"

class UTextBlock;

UENUM(BlueprintType)
enum class ENPScoreFeedbackType : uint8
{
	PhotoPenalty,
	RelicReturnReward
};

/** 사진 감점과 유물 반환 보상을 월드 공간에 표시하는 범용 점수 피드백 위젯입니다. */
UCLASS()
class NOPHOTOS_API UNPScoreFeedbackWidget : public UNPUserWidget
{
	GENERATED_BODY()

public:
	void SetScoreFeedback(int32 Amount, ENPScoreFeedbackType FeedbackType);

private:
	UTextBlock* ResolveFeedbackText() const;

	/** 새 WBP에서 권장하는 TextBlock 이름입니다. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ScoreFeedbackText;

	/** 이름 변경 전 WBP와의 호환을 위해 기존 TextBlock 이름도 지원합니다. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> PhotoPenaltyText;
};
