#pragma once

#include "CoreMinimal.h"
#include "Gameplay/MapEvents/NPMapEvent.h"
#include "NPCCTVMapEvent.generated.h"

class URectLightComponent;
class ANPStablePhysicsPawn;

UCLASS(Blueprintable)
class NOPHOTOS_API ANPCCTVMapEvent : public ANPMapEvent
{
	GENERATED_BODY()

public:
	bool IsTargetReserved(ANPStablePhysicsPawn* Target, float ServerTime) const;
	bool TryReserveTarget(ANPStablePhysicsPawn* Target, float ServerTime, float ReservationDuration);

protected:
	virtual void ApplyEventState_Implementation(bool bNewActive) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 이 태그가 붙은 Rect Light 액터 또는 컴포넌트는 이벤트 중에도 기존 밝기를 유지합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CCTV Event|Lighting")
	FName ExcludedRectLightTag = TEXT("CCTVKeepOn");

private:
	void SetRectLightsDisabled(bool bDisabled);

	TMap<TWeakObjectPtr<URectLightComponent>, float> SavedRectLightIntensities;
	TMap<TWeakObjectPtr<ANPStablePhysicsPawn>, float> TargetReservationEndTimes;
};
