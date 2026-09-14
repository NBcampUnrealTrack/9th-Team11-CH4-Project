#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NPRelicUsePromptWidget.generated.h"

class UImage;
class UMaterialInstanceDynamic;

/** 사용 가능한 유물을 오른손에 들었을 때 표시되는 F키 및 쿨타임 HUD입니다. */
UCLASS(Abstract, Blueprintable)
class NOPHOTOS_API UNPRelicUsePromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetRelicUseDisplay(
		bool bVisible,
		float CooldownProgress,
		float RemainingTime,
		float Duration);

protected:
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category="Relic Use|UI",
		meta=(DisplayName="On Prompt Visibility Changed"))
	void BP_OnPromptVisibilityChanged(bool bVisible);

	/** CooldownProgress는 사용 직후 0, 다시 사용할 수 있을 때 1입니다. */
	UFUNCTION(BlueprintImplementableEvent, Category="Relic Use|UI",
		meta=(DisplayName="On Cooldown Progress Changed"))
	void BP_OnCooldownProgressChanged(
		float CooldownProgress,
		float RemainingTime,
		float Duration);

private:
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> CooldownFillImage;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CooldownMaterial;

	bool bPromptVisible = false;
};
