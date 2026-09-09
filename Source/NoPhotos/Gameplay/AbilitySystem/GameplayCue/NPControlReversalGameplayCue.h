#pragma once

#include "CoreMinimal.h"
#include "Gameplay/AbilitySystem/GameplayCue/NPAttachedGameplayCueActor.h"
#include "NPControlReversalGameplayCue.generated.h"

class UInstancedStaticMeshComponent;

/** 조작 반전 상태 동안 유령 네 개가 캐릭터 주변을 선회합니다. */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPControlReversalGameplayCue : public ANPAttachedGameplayCueActor
{
	GENERATED_BODY()

public:
	ANPControlReversalGameplayCue();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	void SetManagedScaleMultiplier(float ScaleMultiplier);
	void CompleteManagedRemoval();

protected:
	virtual bool WhileActive_Implementation(
		AActor* Target, const FGameplayCueParameters& Parameters) override;
	virtual bool OnRemove_Implementation(
		AActor* Target, const FGameplayCueParameters& Parameters) override;
	virtual bool Recycle() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Control Reversal|Visual")
	TObjectPtr<UInstancedStaticMeshComponent> GhostMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Control Reversal|Visual",
		meta=(DisplayName="캐릭터 중심 거리", ClampMin="1.0", Units="cm"))
	float OrbitRadius = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Control Reversal|Visual", meta=(Units="cm"))
	float OrbitHeight = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Control Reversal|Visual",
		meta=(ClampMin="0.0", Units="deg/s"))
	float OrbitSpeed = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Control Reversal|Visual",
		meta=(DisplayName="상하 진폭", ClampMin="0.0", Units="cm"))
	float BobAmplitude = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Control Reversal|Visual")
	FVector GhostScale = FVector(0.25f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Control Reversal|Visual",
		meta=(DisplayName="유령 메시 회전 보정"))
	FRotator GhostRotationOffset = FRotator::ZeroRotator;

private:
	void RebuildGhostInstances();
	void UpdateGhostTransforms();

	float OrbitAngle = 0.0f;
	float ScaleMultiplier = 1.0f;
};
