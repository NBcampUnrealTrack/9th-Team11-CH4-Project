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
		ObservedRelic = Relic;
		Relic->OnRelicValueChanged.AddUObject(this, &ThisClass::HandleRelicValueChanged);
		RefreshRelicScore();
	}
}

void UNPRelicInfoWidget::ResetRelicInfo()
{
	UnbindObservedRelic();

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
	StopAllAnimations();

	if (IsValid(ShowAnimation))
	{
		PlayAnimation(ShowAnimation, 0.0f, 1, EUMGSequencePlayMode::Forward);
	}
}

void UNPRelicInfoWidget::OnPopRequested_Implementation()
{
	StopAllAnimations();

	if (IsValid(OutAnimation))
	{
		PlayAnimation(OutAnimation, 0.0f, 1, EUMGSequencePlayMode::Forward);
	}
}

void UNPRelicInfoWidget::NativeDestruct()
{
	UnbindObservedRelic();
	Super::NativeDestruct();
}

void UNPRelicInfoWidget::RefreshRelicScore()
{
	if (IsValid(RelicScoreText) && ObservedRelic.IsValid())
	{
		RelicScoreText->SetText(FText::AsNumber(ObservedRelic->GetCurrentPrice()));
	}
}

void UNPRelicInfoWidget::HandleRelicValueChanged(ANPBaseRelic* Relic)
{
	if (Relic == ObservedRelic.Get())
	{
		RefreshRelicScore();
	}
}

void UNPRelicInfoWidget::UnbindObservedRelic()
{
	if (ObservedRelic.IsValid())
	{
		ObservedRelic->OnRelicValueChanged.RemoveAll(this);
	}
	ObservedRelic.Reset();
}
