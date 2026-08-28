#include "UI/GameScreen/Relic/NPRelicInfoWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Components/TextBlock.h"
#include "Gameplay/Relic/NPBaseRelic.h"

#define LOCTEXT_NAMESPACE "NPRelicInfoWidget"

void UNPRelicInfoWidget::SetRelicInfo(ANPBaseRelic* Relic)
{
	ResetRelicInfo();

	if (!IsValid(Relic))
	{
		return;
	}

	// 유물명, 설명은 없길래 일단 가격만 설정해둠
	const int32 Price = Relic->GetBasePrice();
	if (Price > 0 && IsValid(RelicScoreText))
	{
		RelicScoreText->SetText(FText::AsNumber(Price));
	}
}

void UNPRelicInfoWidget::ResetRelicInfo()
{
	if (IsValid(RelicNameText))
	{
		RelicNameText->SetText(LOCTEXT("DefaultRelicName", "유물명"));
	}

	if (IsValid(RelicScoreText))
	{
		RelicScoreText->SetText(LOCTEXT("DefaultRelicPrice", "가격"));
	}

	if (IsValid(RelicDescriptionText))
	{
		RelicDescriptionText->SetText(LOCTEXT("DefaultRelicDescription", "유물 설명"));
	}
}

void UNPRelicInfoWidget::PlayShowAnimation()
{
	if (IsValid(ShowAnimation))
	{
		PlayAnimation(ShowAnimation, 0.0f, 1, EUMGSequencePlayMode::Forward);
	}
}

void UNPRelicInfoWidget::OnPopRequested_Implementation()
{
	if (IsValid(OutAnimation))
	{
		PlayAnimation(OutAnimation, 0.0f, 1, EUMGSequencePlayMode::Forward);
	}
}

#undef LOCTEXT_NAMESPACE
