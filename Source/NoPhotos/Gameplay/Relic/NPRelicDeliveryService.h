#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "NPRelicDeliveryService.generated.h"

class ANPBaseRelic;
class ANPMainGameMode;
class ANPRelicReturnZone;
class APlayerState;

/** 서버에서 Relic의 증거 누적, 반환 가치 계산과 최종 점수 지급을 처리합니다. */
UCLASS()
class NOPHOTOS_API UNPRelicDeliveryService : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(ANPMainGameMode* InGameMode);
	virtual UWorld* GetWorld() const override;

	bool RegisterPhotoEvidence(ANPBaseRelic* Relic, APlayerState* Photographer);
	bool TryDeliverRelic(ANPBaseRelic* Relic, ANPRelicReturnZone* ReturnZone);

	UFUNCTION(BlueprintPure, Category="Relic|Delivery")
	int32 CalculateReturnScore(const ANPBaseRelic* Relic) const;

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<ANPMainGameMode> OwningGameMode;

	/** 유효한 증거 사진 한 장마다 Relic 기본 가격에서 누적 차감할 비율입니다. */
	UPROPERTY(EditDefaultsOnly, Category="Relic|Delivery",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float PhotoPenaltyRatePerCapture = 0.1f;
};
