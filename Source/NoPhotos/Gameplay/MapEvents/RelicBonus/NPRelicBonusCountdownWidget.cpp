#include "NPRelicBonusCountdownWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"

TSharedRef<SWidget> UNPRelicBonusCountdownWidget::RebuildWidget()
{
	// NativeConstruct 시점에는 Slate 위젯이 이미 만들어진 뒤이므로,
	// 네이티브 전용 WidgetTree는 반드시 Super::RebuildWidget 전에 구성합니다.
	BuildDefaultWidget();
	return Super::RebuildWidget();
}

void UNPRelicBonusCountdownWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetRemainingSeconds(CachedRemainingSeconds == INDEX_NONE ? 0 : CachedRemainingSeconds);
}

void UNPRelicBonusCountdownWidget::SetRemainingSeconds(const int32 RemainingSeconds)
{
	CachedRemainingSeconds = FMath::Max(0, RemainingSeconds);
	if (IsValid(RemainingTimeText))
	{
		RemainingTimeText->SetText(FText::Format(
			NSLOCTEXT("RelicBonus", "RemainingSeconds", "{0}초"),
			FText::AsNumber(CachedRemainingSeconds)));
	}
}

void UNPRelicBonusCountdownWidget::BuildDefaultWidget()
{
	if (!WidgetTree || IsValid(RemainingTimeText))
	{
		return;
	}

	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("CountdownBackground"));
	RemainingTimeText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("RemainingTimeText"));
	if (!Background || !RemainingTimeText)
	{
		return;
	}

	WidgetTree->RootWidget = Background;
	Background->SetBrushColor(FLinearColor(0.01f, 0.01f, 0.01f, 0.65f));
	Background->SetPadding(FMargin(12.0f, 6.0f));
	Background->SetContent(RemainingTimeText);

	FSlateFontInfo FontInfo = RemainingTimeText->GetFont();
	FontInfo.Size = 28;
	RemainingTimeText->SetFont(FontInfo);
	RemainingTimeText->SetJustification(ETextJustify::Center);
	RemainingTimeText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.82f, 0.15f)));
	RemainingTimeText->SetShadowOffset(FVector2D(2.0f, 2.0f));
	RemainingTimeText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f));
}
