#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NPAimCrosshairWidget.generated.h"

/** 로컬 플레이어가 조준 유물로 조준하는 동안 표시되는 HUD 조준점입니다. */
UCLASS(Abstract, Blueprintable)
class NOPHOTOS_API UNPAimCrosshairWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** GAS의 State.Relic.Aiming 상태를 화면 표시 상태에 반영합니다. */
	UFUNCTION(BlueprintCallable, Category = "Aim|UI")
	void SetAimActive(bool bActive);

protected:
	/** 블루프린트에서 조준점 등장/퇴장 애니메이션이 필요할 때 사용합니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Aim|UI", meta = (DisplayName = "On Aim Active Changed"))
	void BP_OnAimActiveChanged(bool bActive);
};
