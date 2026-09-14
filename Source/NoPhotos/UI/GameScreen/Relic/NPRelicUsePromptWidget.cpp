#include "UI/GameScreen/Relic/NPRelicUsePromptWidget.h"

#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"

void UNPRelicUsePromptWidget::NativeConstruct()
{
	Super::NativeConstruct();

	CooldownMaterial = IsValid(CooldownFillImage)
		? CooldownFillImage->GetDynamicMaterial()
		: nullptr;
}

void UNPRelicUsePromptWidget::SetRelicUseDisplay(
	const bool bVisible,
	const float CooldownProgress,
	const float RemainingTime,
	const float Duration)
{
	if (bPromptVisible != bVisible)
	{
		bPromptVisible = bVisible;
		BP_OnPromptVisibilityChanged(bVisible);
	}
	SetVisibility(bVisible
		? ESlateVisibility::SelfHitTestInvisible
		: ESlateVisibility::Collapsed);

	if (!IsValid(CooldownMaterial) && IsValid(CooldownFillImage))
	{
		CooldownMaterial = CooldownFillImage->GetDynamicMaterial();
	}
	const float SafeCooldownProgress = FMath::Clamp(
		CooldownProgress,
		0.0f,
		1.0f);
	if (IsValid(CooldownMaterial))
	{
		CooldownMaterial->SetScalarParameterValue(
			TEXT("Progress"),
			SafeCooldownProgress);
	}
	BP_OnCooldownProgressChanged(
		SafeCooldownProgress,
		FMath::Max(0.0f, RemainingTime),
		FMath::Max(0.0f, Duration));
}
