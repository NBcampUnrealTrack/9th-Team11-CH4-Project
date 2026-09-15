#include "UI/Loading/NPGameStartCountdownWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"

TSharedRef<SWidget> UNPGameStartCountdownWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder* Root = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(),
			TEXT("CountdownRoot"));
		Root->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.35f));
		Root->SetHorizontalAlignment(HAlign_Center);
		Root->SetVerticalAlignment(VAlign_Center);

		CountdownText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			TEXT("CountdownText"));
		CountdownText->SetJustification(ETextJustify::Center);
		CountdownText->SetColorAndOpacity(
			FSlateColor(FLinearColor::White));
		FSlateFontInfo Font = CountdownText->GetFont();
		Font.Size = 72;
		CountdownText->SetFont(Font);
		Root->SetContent(CountdownText);
		WidgetTree->RootWidget = Root;
	}

	return Super::RebuildWidget();
}

void UNPGameStartCountdownWidget::SetCountdownStep(const int32 Step)
{
	const int32 SafeStep = FMath::Clamp(Step, 0, 3);
	if (CurrentStep == SafeStep)
	{
		return;
	}

	CurrentStep = SafeStep;
	FText DisplayText = GameStartText;
	if (SafeStep > 0)
	{
		DisplayText = FText::AsNumber(SafeStep);
	}
	if (CountdownText)
	{
		CountdownText->SetText(DisplayText);
	}
	BP_OnCountdownStepChanged(SafeStep, DisplayText);
}
