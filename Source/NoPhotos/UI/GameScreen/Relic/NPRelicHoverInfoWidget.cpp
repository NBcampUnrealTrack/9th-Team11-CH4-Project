#include "UI/GameScreen/Relic/NPRelicHoverInfoWidget.h"
#include "Components/TextBlock.h"

void UNPRelicHoverInfoWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UNPRelicHoverInfoWidget::SetRelicInfo(const FText& InRelicName, const int32 InPrice)
{
	if (IsValid(RelicNameText))
	{
		RelicNameText->SetText(InRelicName.IsEmpty() ? FText::FromString(TEXT("이름 없는 유물")) : InRelicName);
	}

	if (IsValid(RelicScoreText))
	{
		RelicScoreText->SetText(FText::AsNumber(FMath::Max(0, InPrice)));
	}
}

void UNPRelicHoverInfoWidget::ResetRelicInfo()
{
	if (IsValid(RelicNameText))
	{
		RelicNameText->SetText(FText::FromString(TEXT("유물명")));
	}

	if (IsValid(RelicScoreText))
	{
		RelicScoreText->SetText(FText::FromString(TEXT("가격")));
	}
}
