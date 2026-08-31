#pragma once

#include "CoreMinimal.h"
#include "Gameplay/MapEvents/NPMapEvent.h"
#include "NPSpeedBoostMapEvent.generated.h"

class ANPStablePhysicsPawn;

UCLASS(Blueprintable)
class NOPHOTOS_API ANPSpeedBoostMapEvent : public ANPMapEvent
{
	GENERATED_BODY()

public:
	ANPSpeedBoostMapEvent();

protected:
	virtual void ApplyEventState_Implementation(bool bNewActive) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 이벤트가 활성화된 동안 적용할 이동속도 배율입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Speed Boost", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float SpeedMultiplier = 2.0f;

	/** 캐릭터 프로필에 설정한 평상시 최대 이동속도와 같은 값으로 맞춥니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Speed Boost", meta = (ClampMin = "0.0", Units = "cm/s"))
	float NormalMoveSpeed = 350.0f;

private:
	void RefreshAffectedPlayers();
	void RestoreAffectedPlayers();
	void ApplySpeed(ANPStablePhysicsPawn* Pawn, float MoveSpeed);

	TSet<TWeakObjectPtr<ANPStablePhysicsPawn>> AffectedPawns;
	FTimerHandle PlayerRefreshTimer;
};
