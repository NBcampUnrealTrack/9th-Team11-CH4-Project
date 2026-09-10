#include "UI/GameScreen/NPScoreFeedbackWidget.h"

#include "Components/TextBlock.h"
#include "Gameplay/Photo/NPPhotoLog.h"

namespace NPScoreFeedback
{
const FLinearColor PhotoPenaltyColor(1.0f, 0.08f, 0.05f, 1.0f);
const FLinearColor RelicRewardColor(0.05f, 1.0f, 0.15f, 1.0f);
}

void UNPScoreFeedbackWidget::SetScoreFeedback(
	const int32 Amount,
	const ENPScoreFeedbackType FeedbackType)
{
	UTextBlock* FeedbackText = ResolveFeedbackText();
	const bool bAllowsZero = FeedbackType == ENPScoreFeedbackType::RelicReturnReward;
	if (!IsValid(FeedbackText) || Amount < 0 || (!bAllowsZero && Amount == 0))
	{
		UE_LOG(
			LogNPPhoto,
			Warning,
			TEXT("[ScoreFeedbackUI] Cannot update text. Widget=%s TextBlock=%s Amount=%d Type=%d"),
			*GetNameSafe(this),
			*GetNameSafe(FeedbackText),
			Amount,
			static_cast<int32>(FeedbackType));
		return;
	}

	if (FeedbackType == ENPScoreFeedbackType::PhotoPenalty)
	{
		FeedbackText->SetText(FText::Format(
			NSLOCTEXT(
				"NPScoreFeedback",
				"RelicValueReducedByAmount",
				"유물 가치 -{0}점"),
			FText::AsNumber(Amount)));
		FeedbackText->SetColorAndOpacity(FSlateColor(NPScoreFeedback::PhotoPenaltyColor));
		return;
	}

	if (FeedbackType == ENPScoreFeedbackType::PersonalMissionBonus)
	{
		FeedbackText->SetText(FText::Format(
			MissionBonusTextFormat,
			FText::AsNumber(Amount)));
		FeedbackText->SetColorAndOpacity(FSlateColor(MissionBonusColor));
		return;
	}

	FeedbackText->SetText(FText::Format(
		NSLOCTEXT(
			"NPScoreFeedback",
			"RelicReturnRewardByAmount",
			"+{0}점"),
		FText::AsNumber(Amount)));
	FeedbackText->SetColorAndOpacity(FSlateColor(NPScoreFeedback::RelicRewardColor));
}

UTextBlock* UNPScoreFeedbackWidget::ResolveFeedbackText() const
{
	return IsValid(ScoreFeedbackText)
		? ScoreFeedbackText.Get()
		: PhotoPenaltyText.Get();
}
