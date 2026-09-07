#include "UI/Loading/NPMainWorldLoadingWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"

TSharedRef<SWidget> UNPMainWorldLoadingWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder* Background = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(),
			TEXT("LoadingBackground"));
		Background->SetBrushColor(FLinearColor(0.01f, 0.01f, 0.015f, 0.96f));
		Background->SetHorizontalAlignment(HAlign_Center);
		Background->SetVerticalAlignment(VAlign_Center);

		LoadingStatusText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			TEXT("LoadingStatusText"));
		LoadingStatusText->SetText(LoadingText);
		LoadingStatusText->SetJustification(ETextJustify::Center);
		LoadingStatusText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		FSlateFontInfo Font = LoadingStatusText->GetFont();
		Font.Size = 28;
		LoadingStatusText->SetFont(Font);

		Background->SetContent(LoadingStatusText);
		WidgetTree->RootWidget = Background;
	}

	return Super::RebuildWidget();
}

void UNPMainWorldLoadingWidget::ShowLoading()
{
	SetVisibility(ESlateVisibility::Visible);
	if (LoadingStatusText)
	{
		LoadingStatusText->SetText(LoadingText);
	}
}

void UNPMainWorldLoadingWidget::ShowFailure()
{
	SetVisibility(ESlateVisibility::Visible);
	if (LoadingStatusText)
	{
		LoadingStatusText->SetText(FailureText);
		LoadingStatusText->SetColorAndOpacity(
			FSlateColor(FLinearColor(1.0f, 0.15f, 0.1f)));
	}
}
