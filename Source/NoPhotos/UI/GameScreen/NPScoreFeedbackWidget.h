#pragma once

#include "CoreMinimal.h"
#include "UI/NPUserWidget.h"
#include "NPScoreFeedbackWidget.generated.h"

class UTextBlock;

UENUM(BlueprintType)
enum class ENPScoreFeedbackType : uint8
{
	PhotoPenalty,
	RelicReturnReward,
	PersonalMissionBonus
};

/** 사진 감점, 유물 반환 보상과 개인 미션 보너스를 월드 공간에 표시합니다. */
UCLASS()
class NOPHOTOS_API UNPScoreFeedbackWidget : public UNPUserWidget
{
	GENERATED_BODY()

public:
	void SetScoreFeedback(int32 Amount, ENPScoreFeedbackType FeedbackType);

private:
	UTextBlock* ResolveFeedbackText() const;

	/** {0} 위치에 개인 미션 보너스 점수가 들어갑니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Score Feedback|Mission Bonus",
		meta=(AllowPrivateAccess="true"))
	FText MissionBonusTextFormat = INVTEXT("개인 미션 +{0}점");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Score Feedback|Mission Bonus",
		meta=(AllowPrivateAccess="true"))
	FLinearColor MissionBonusColor = FLinearColor(1.0f, 0.65f, 0.05f, 1.0f);

	/** 새 WBP에서 권장하는 TextBlock 이름입니다. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ScoreFeedbackText;

	/** 이름 변경 전 WBP와의 호환을 위해 기존 TextBlock 이름도 지원합니다. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> PhotoPenaltyText;
};
