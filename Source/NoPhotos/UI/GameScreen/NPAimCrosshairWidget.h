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

	/** 사진 촬영 쿨다운을 배터리 UI로 표현할 수 있도록 현재 충전 상태를 전달합니다. */
	UFUNCTION(BlueprintCallable, Category = "Aim|UI")
	void SetCooldownDisplay(
		int32 ChargedCellCount,
		int32 MaximumCellCount,
		float RemainingTime,
		float Duration);

protected:
	/** 블루프린트에서 조준점 등장/퇴장 애니메이션이 필요할 때 사용합니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Aim|UI", meta = (DisplayName = "On Aim Active Changed"))
	void BP_OnAimActiveChanged(bool bActive);

	/** WBP_NPPhotoAim에서 배터리 칸과 필요하다면 남은 시간 표시를 갱신합니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Aim|UI", meta = (DisplayName = "On Cooldown Display Changed"))
	void BP_OnCooldownDisplayChanged(
		int32 ChargedCellCount,
		int32 MaximumCellCount,
		float RemainingTime,
		float Duration);
};
