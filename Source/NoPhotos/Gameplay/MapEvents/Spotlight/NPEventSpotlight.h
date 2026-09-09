#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NPEventSpotlight.generated.h"

class ANPStablePhysicsPawn;
class USpotLightComponent;
class UStaticMeshComponent;

/** 고정 위치에서 조사 방향을 좌우로 회전하는 이벤트 조명입니다. */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPEventSpotlight : public AActor
{
	GENERATED_BODY()

public:
	ANPEventSpotlight();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 포인트 스케일은 조명 대신 빛기둥 메시의 기본 크기에만 곱합니다. */
	void SetBeamScaleMultiplier(const FVector& Multiplier);
	void UpdateBeam(float ElapsedSeconds, bool bActive);
	bool IsIlluminatingPawn(const ANPStablePhysicsPawn* Pawn, const AActor* HeldRelic) const;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpotLightComponent> Spotlight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BeamMesh;

	/** 배치된 포인트의 로컬 X축을 중심으로 좌우로 기울이는 최대 각도입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spotlight", meta = (ClampMin = "0.0", ClampMax = "80.0", Units = "deg"))
	float SweepAngle = 25.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spotlight", meta = (ClampMin = "0.1", Units = "s"))
	float SweepPeriod = 8.0f;

private:
	void UpdateBeamGeometry();

	UFUNCTION()
	void OnRep_BeamScaleMultiplier();

	UPROPERTY(ReplicatedUsing = OnRep_BeamScaleMultiplier)
	FVector BeamScaleMultiplier = FVector::OneVector;

	FVector InitialBeamScale = FVector::OneVector;
	FQuat InitialLightRotation = FQuat::Identity;
};
