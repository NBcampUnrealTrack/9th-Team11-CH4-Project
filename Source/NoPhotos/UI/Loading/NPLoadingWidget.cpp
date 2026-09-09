#include "UI/Loading/NPLoadingWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "SubSystem/Room/NPRoomGenerateSubsystem.h"

void UNPLoadingWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UpdateLoadingProgress();
}

void UNPLoadingWidget::NativeTick(
	const FGeometry& MyGeometry,
	const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateLoadingProgress();
}

void UNPLoadingWidget::UpdateLoadingProgress()
{
	const UNPRoomGenerateSubsystem* RoomGenerator = GetWorld()
		? GetWorld()->GetSubsystem<UNPRoomGenerateSubsystem>()
		: nullptr;
	const float Progress = RoomGenerator
		? FMath::Clamp(RoomGenerator->GetGenerationProgress(), 0.0f, 1.0f)
		: 0.0f;

	if (LoadingProgressBar)
	{
		LoadingProgressBar->SetPercent(Progress);
	}

	if (LoadingPercentText)
	{
		const int32 Percent = FMath::RoundToInt(Progress * 100.0f);
		LoadingPercentText->SetText(FText::Format(
			NSLOCTEXT("NoPhotos", "LoadingProgressPercent", "{0}%"),
			FText::AsNumber(Percent)));
	}
}
