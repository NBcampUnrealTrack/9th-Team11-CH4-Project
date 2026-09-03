#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NPPhotoCapturePenaltyComponent.generated.h"

class ANPBaseRelic;
class FLifetimeProperty;
class UMaterialInterface;

/** 유물 증거 사진에 찍힌 캐릭터의 강제 Drop, 감속 및 외형 피드백을 관리합니다. */
UCLASS(ClassGroup=(Photo), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPPhotoCapturePenaltyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNPPhotoCapturePenaltyComponent();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 서버에서 증거 사진에 포함된 유물과 현재 보유 유물이 같은 경우 패널티를 적용합니다. */
	bool ApplyCapturedWithRelicPenalty(ANPBaseRelic* EvidenceRelic);

	UFUNCTION(BlueprintPure, Category="Photo Penalty")
	bool IsPhotoSlowActive() const { return bPhotoSlowActive; }

	UFUNCTION(BlueprintPure, Category="Photo Penalty")
	float GetPhotoSlowEndServerTime() const { return PhotoSlowEndServerTime; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 0.8은 원래 이동 속도의 80%를 의미합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Photo Penalty|Movement",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float MoveSpeedMultiplier = 0.8f;

	/** 마지막 증거 사진 성공 시점부터 유지되는 감속 시간입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Photo Penalty|Movement",
		meta=(ClampMin="0.01", Units="s"))
	float SlowDuration = 2.5f;

	/** 감속 중 원래 캐릭터 재질 위에 표시할 흰색 Overlay Material입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Photo Penalty|Visual")
	TObjectPtr<UMaterialInterface> SlowOverlayMaterial;

private:
	UFUNCTION()
	void OnRep_PhotoSlowActive();

	void ApplySlowStateLocally();
	void FinishSlowPenalty();

	UPROPERTY(ReplicatedUsing=OnRep_PhotoSlowActive)
	bool bPhotoSlowActive = false;

	UPROPERTY(Replicated)
	float PhotoSlowEndServerTime = 0.0f;

	FTimerHandle SlowTimer;
};
