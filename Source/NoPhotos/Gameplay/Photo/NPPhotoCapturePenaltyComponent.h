#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "NPPhotoCapturePenaltyComponent.generated.h"

class ANPBaseRelic;
class UNPAbilitySystemComponent;

/** 유물 증거 사진에 찍힌 캐릭터의 강제 Drop과 일시적인 조작 불가 상태를 관리합니다. */
UCLASS(ClassGroup=(Photo), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPPhotoCapturePenaltyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNPPhotoCapturePenaltyComponent();

	/** 서버에서 증거 사진에 포함된 유물과 현재 보유 유물이 같은 경우 패널티를 적용합니다. */
	bool ApplyCapturedWithRelicPenalty(
		ANPBaseRelic* EvidenceRelic,
		int32 AppliedPhotoPenalty);

	UFUNCTION(BlueprintPure, Category="Photo Penalty")
	bool IsPhotoStunActive() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 효과가 새로 적용될 때마다 이 시간으로 갱신되는 조작 차단 시간입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Photo Penalty|Stun",
		meta=(ClampMin="0.01", Units="s"))
	float StunDuration = 1.0f;

	/** 머리 위 가격 감소 알림이 유지되는 시간입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Photo Penalty|Visual",
		meta=(ClampMin="0.01", Units="s"))
	float PriceReductionMessageDuration = 2.0f;

private:
	void HandleStunTagChanged(const FGameplayTag StunTag, int32 NewCount);
	void ApplyStunStateLocally(bool bStunned);
	UNPAbilitySystemComponent* ResolveAbilitySystem() const;

	FDelegateHandle StunTagChangedHandle;
};
