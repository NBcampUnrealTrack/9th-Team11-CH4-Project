#include "UI/GameScreen/NPAimCrosshairWidget.h"

void UNPAimCrosshairWidget::SetAimActive(const bool bActive)
{
	SetVisibility(bActive
		? ESlateVisibility::SelfHitTestInvisible
		: ESlateVisibility::Collapsed);
	BP_OnAimActiveChanged(bActive);
}

void UNPAimCrosshairWidget::SetCooldownDisplay(
	const int32 ChargedCellCount,
	const int32 MaximumCellCount,
	const float RemainingTime,
	const float Duration)
{
	const int32 SafeMaximumCellCount = FMath::Max(1, MaximumCellCount);
	BP_OnCooldownDisplayChanged(
		FMath::Clamp(ChargedCellCount, 0, SafeMaximumCellCount),
		SafeMaximumCellCount,
		FMath::Max(0.0f, RemainingTime),
		FMath::Max(0.0f, Duration));
}
