#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NPPhotoCapturePenaltyComponent.generated.h"

class ANPBaseRelic;
class FLifetimeProperty;

/** 유물 증거 사진에 찍힌 캐릭터의 강제 Drop과 일시적인 조작 불가 상태를 관리합니다. */
UCLASS(ClassGroup=(Photo), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPPhotoCapturePenaltyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNPPhotoCapturePenaltyComponent();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 서버에서 증거 사진에 포함된 유물과 현재 보유 유물이 같은 경우 패널티를 적용합니다. */
	bool ApplyCapturedWithRelicPenalty(
		ANPBaseRelic* EvidenceRelic,
		int32 AppliedPhotoPenalty);

	UFUNCTION(BlueprintPure, Category="Photo Penalty")
	bool IsPhotoStunActive() const { return bPhotoStunActive; }

	UFUNCTION(BlueprintPure, Category="Photo Penalty")
	float GetPhotoStunEndServerTime() const { return PhotoStunEndServerTime; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 마지막 증거 사진 성공 시점부터 조작을 차단하는 시간입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Photo Penalty|Stun",
		meta=(ClampMin="0.01", Units="s"))
	float StunDuration = 1.0f;

	/** 머리 위 가격 감소 알림이 유지되는 시간입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Photo Penalty|Visual",
		meta=(ClampMin="0.01", Units="s"))
	float PriceReductionMessageDuration = 2.0f;

private:
	UFUNCTION()
	void OnRep_PhotoStunActive();

	void ApplyStunStateLocally();
	void FinishStunPenalty();

	UPROPERTY(ReplicatedUsing=OnRep_PhotoStunActive)
	bool bPhotoStunActive = false;

	UPROPERTY(Replicated)
	float PhotoStunEndServerTime = 0.0f;

	FTimerHandle StunTimer;
};
