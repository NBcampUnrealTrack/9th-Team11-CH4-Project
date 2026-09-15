#include "UI/Loading/NPMainWorldLoadingWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

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

		UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(), TEXT("LoadingContent"));
		Content->AddChildToVerticalBox(LoadingStatusText);

		ShaderCacheProgressBar = WidgetTree->ConstructWidget<UProgressBar>(
			UProgressBar::StaticClass(), TEXT("ShaderCacheProgressBar"));
		ShaderCacheProgressBar->SetFillColorAndOpacity(FLinearColor(0.2f, 0.65f, 1.0f));
		ShaderCacheProgressBox = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(), TEXT("ShaderCacheProgressBox"));
		ShaderCacheProgressBox->SetWidthOverride(480.0f);
		ShaderCacheProgressBox->SetHeightOverride(12.0f);
		ShaderCacheProgressBox->SetContent(ShaderCacheProgressBar);
		ShaderCacheProgressBox->SetVisibility(ESlateVisibility::Collapsed);
		Content->AddChildToVerticalBox(ShaderCacheProgressBox)->SetPadding(FMargin(0.0f, 20.0f, 0.0f, 12.0f));

		ShaderCacheProgressText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("ShaderCacheProgressText"));
		ShaderCacheProgressText->SetJustification(ETextJustify::Center);
		ShaderCacheProgressText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		Font.Size = 18;
		ShaderCacheProgressText->SetFont(Font);
		ShaderCacheProgressText->SetVisibility(ESlateVisibility::Collapsed);
		Content->AddChildToVerticalBox(ShaderCacheProgressText);

		Background->SetContent(Content);
		WidgetTree->RootWidget = Background;
	}

	return Super::RebuildWidget();
}

void UNPMainWorldLoadingWidget::SetLoadingText(const FText& InLoadingText)
{
	LoadingText = InLoadingText;
	ShowLoading();
}

void UNPMainWorldLoadingWidget::SetShaderCacheProgress(
	float EstimatedProgress, uint32 Remaining, double ElapsedSeconds, double RecentTasksPerSecond)
{
	if (!ShaderCacheProgressBar || !ShaderCacheProgressBox || !ShaderCacheProgressText)
	{
		return;
	}

	const float Progress = FMath::Clamp(EstimatedProgress, 0.0f, 1.0f);
	ShaderCacheProgressBox->SetVisibility(ESlateVisibility::Visible);
	ShaderCacheProgressBar->SetPercent(Progress);
	ShaderCacheProgressText->SetVisibility(ESlateVisibility::Visible);
	FNumberFormattingOptions NumberFormat;
	NumberFormat.SetMinimumFractionalDigits(1);
	NumberFormat.SetMaximumFractionalDigits(1);
	const FText SpeedText = RecentTasksPerSecond >= 0.0
		? FText::AsNumber(RecentTasksPerSecond, &NumberFormat)
		: FText::FromString(TEXT("--"));
	ShaderCacheProgressText->SetText(FText::Format(
		NSLOCTEXT("NoPhotos", "ShaderCacheMetrics", "Elapsed: {0}s\nRemaining: {1}\nRecent speed: {2} tasks/s"),
		FText::AsNumber(ElapsedSeconds, &NumberFormat), FText::AsNumber(Remaining), SpeedText));
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
