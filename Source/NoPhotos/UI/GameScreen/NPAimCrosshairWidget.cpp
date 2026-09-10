#include "UI/GameScreen/NPAimCrosshairWidget.h"

void UNPAimCrosshairWidget::SetAimActive(const bool bActive)
{
	SetVisibility(bActive
		? ESlateVisibility::SelfHitTestInvisible
		: ESlateVisibility::Collapsed);
	BP_OnAimActiveChanged(bActive);
}
