#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NPEventSpotlight.generated.h"

class ANPStablePhysicsPawn;
class USpotLightComponent;

/** 고정 위치에서 조사 방향을 좌우로 회전하는 이벤트 조명입니다. */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPEventSpotlight : public AActor
{
	GENERATED_BODY()

public:
	ANPEventSpotlight();
	virtual void Tick(float DeltaSeconds) override;

	void UpdateBeam(float ElapsedSeconds, bool bActive);
	bool IsIlluminatingPawn(const ANPStablePhysicsPawn* Pawn, const AActor* HeldRelic) const;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpotLightComponent> Spotlight;

	/** 배치된 포인트의 로컬 X축을 중심으로 좌우로 기울이는 최대 각도입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spotlight", meta = (ClampMin = "0.0", ClampMax = "80.0", Units = "deg"))
	float SweepAngle = 25.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spotlight", meta = (ClampMin = "0.1", Units = "s"))
	float SweepPeriod = 8.0f;

private:
	FQuat InitialLightRotation = FQuat::Identity;
};
