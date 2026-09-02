#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NPWaterFlow.generated.h"

class ANPStablePhysicsPawn;
class UArrowComponent;
class UBoxComponent;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMeshComponent;

/** 윗면에 올라선 물리 캐릭터를 FlowDirection 방향으로 떠내려가게 하는 물 큐브입니다. */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPWaterFlow : public AActor
{
	GENERATED_BODY()

public:
	ANPWaterFlow();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleFlowVolumeBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleFlowVolumeEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Water Flow")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Water Flow")
	TObjectPtr<UStaticMeshComponent> WaterMesh;

	/** 물 큐브 윗면에 맞춰 얇게 배치하는 감지 영역입니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Water Flow")
	TObjectPtr<UBoxComponent> FlowVolume;

	/** 물이 흐르는 방향을 표시하고, 실제 물살 방향으로 사용합니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Water Flow")
	TObjectPtr<UArrowComponent> FlowDirection;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water Flow", meta = (ClampMin = "0.0", Units = "cm/s"))
	float FlowSpeed = 220.0f;

private:
	void AddAffectedPawn(ANPStablePhysicsPawn* Pawn);
	void RemoveAffectedPawn(ANPStablePhysicsPawn* Pawn);
	FVector GetFlowVelocity() const;

	TSet<TWeakObjectPtr<ANPStablePhysicsPawn>> AffectedPawns;
};
