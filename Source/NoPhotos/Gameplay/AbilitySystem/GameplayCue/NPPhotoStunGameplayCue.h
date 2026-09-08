#pragma once

#include "CoreMinimal.h"
#include "Gameplay/AbilitySystem/GameplayCue/NPAttachedGameplayCueActor.h"
#include "NPPhotoStunGameplayCue.generated.h"

class UInstancedStaticMeshComponent;

/** 사진 스턴 상태 동안 캐릭터 머리 위에서 표시물 세 개가 회전합니다. */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPPhotoStunGameplayCue : public ANPAttachedGameplayCueActor
{
	GENERATED_BODY()

public:
	ANPPhotoStunGameplayCue();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual bool WhileActive_Implementation(
		AActor* Target, const FGameplayCueParameters& Parameters) override;
	virtual bool OnRemove_Implementation(
		AActor* Target, const FGameplayCueParameters& Parameters) override;
	virtual bool Recycle() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Photo|Stun Visual")
	TObjectPtr<UInstancedStaticMeshComponent> MarkerMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Photo|Stun Visual", meta=(Units="cm"))
	float VisualHeight = 190.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Photo|Stun Visual",
		meta=(ClampMin="0.0", Units="cm"))
	float OrbitRadius = 28.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Photo|Stun Visual")
	FVector MarkerScale = FVector(0.12f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Photo|Stun Visual", meta=(Units="deg/s"))
	float RotationSpeedDegrees = 180.0f;

private:
	void RebuildMarkerInstances();
};
