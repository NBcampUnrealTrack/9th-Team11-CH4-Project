#include "UI/GameScreen/Relic/NPRelicInfoWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Components/TextBlock.h"
#include "Data/Structs/NPRelicData.h"
#include "Gameplay/Relic/NPBaseRelic.h"

void UNPRelicInfoWidget::SetRelicInfo(ANPBaseRelic* Relic)
{
	ResetRelicInfo();

	if (!IsValid(Relic))
	{
		return;
	}

	const FNPRelicTableRow* RelicData = Relic->GetRelicTableData();
	if (!RelicData)
	{
		return;
	}

	if (!RelicData->DisplayName.IsEmpty() && IsValid(RelicNameText))
	{
		RelicNameText->SetText(RelicData->DisplayName);
	}

	if (!RelicData->Description.IsEmpty() && IsValid(RelicDescriptionText))
	{
		RelicDescriptionText->SetText(RelicData->Description);
	}

	if (IsValid(RelicScoreText))
	{
		RelicScoreText->SetText(FText::AsNumber(FMath::Max(0, RelicData->Price)));
	}
}

void UNPRelicInfoWidget::ResetRelicInfo()
{
	if (IsValid(RelicNameText))
	{
		RelicNameText->SetText(FText::FromString(TEXT("유물명")));
	}

	if (IsValid(RelicScoreText))
	{
		RelicScoreText->SetText(FText::FromString(TEXT("가격")));
	}

	if (IsValid(RelicDescriptionText))
	{
		RelicDescriptionText->SetText(FText::FromString(TEXT("유물 설명")));
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