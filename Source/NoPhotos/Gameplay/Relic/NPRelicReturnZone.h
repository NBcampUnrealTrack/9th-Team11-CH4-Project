#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NPRelicReturnZone.generated.h"

class UBoxComponent;
class UPrimitiveComponent;
class USoundBase;
class UStaticMesh;
class ANPBaseRelic;
class ANPRelicDeliveryEffect;
class UNPRelicOwnershipComponent;

/** 레벨에 배치하여 서버에서 Relic 반환 Overlap을 감지하는 구역입니다. */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPRelicReturnZone : public AActor
{
	GENERATED_BODY()

public:
	ANPRelicReturnZone();

	/** RelicBonus 이벤트가 생성한 반환 존에서만 제출 성공 연출을 허용합니다. */
	void SetDeliveryEffectEnabled(bool bEnabled) { bDeliveryEffectEnabled = bEnabled; }

	/** 이 반환 존에 제출할 때 최종 반환 점수에 적용할 배율입니다. */
	UFUNCTION(BlueprintPure, Category="Relic|Delivery")
	float GetReturnScoreMultiplier() const { return ReturnScoreMultiplier; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleReturnVolumeBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	/** 서버에서 유물 제출이 실제로 성공했을 때 원본 유물과 제출 위치를 전달하며 각 클라이언트에서 호출됩니다. */
	UFUNCTION()
	void HandleReturnVolumeEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex);

	/** 서버에서 유물 제출이 실제로 성공했을 때 각 클라이언트에서 호출됩니다. */
	UFUNCTION(BlueprintImplementableEvent, Category="Relic|Delivery", meta=(DisplayName="On Relic Delivered"))
	void BP_OnRelicDelivered(
		ANPBaseRelic* DeliveredRelic,
		FVector DeliveryLocation);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UBoxComponent> ReturnVolume;

	/** 1.0은 일반 점수, 1.5는 50% 추가, 2.0은 두 배 점수입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Relic|Delivery", meta=(ClampMin="0.0", UIMin="0.0"))
	float ReturnScoreMultiplier = 1.0f;

	/** 반환 성공 시 로컬에서 생성할 Niagara 연출 액터입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Relic|Delivery")
	TSubclassOf<ANPRelicDeliveryEffect> DeliveryEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Relic|Delivery Audio", meta=(ClampMin="0"))
	int32 LowPriceThreshold = 250;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Relic|Delivery Audio", meta=(ClampMin="0"))
	int32 MidPriceThreshold = 450;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Relic|Delivery Audio")
	TObjectPtr<USoundBase> LowPriceSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Relic|Delivery Audio")
	TObjectPtr<USoundBase> MidPriceSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Relic|Delivery Audio")
	TObjectPtr<USoundBase> LargePriceSound;

private:
	void RegisterOverlappingRelic(ANPBaseRelic* Relic);
	void UnregisterOverlappingRelic(ANPBaseRelic* Relic);
	void HandleRelicOwnershipChanged(UNPRelicOwnershipComponent* Ownership);
	bool TryDeliverOverlappingRelic(ANPBaseRelic* Relic);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastNotifyRelicDelivered(
		ANPBaseRelic* DeliveredRelic,
		const FTransform& DeliveryTransform,
		UStaticMesh* RelicMesh,
		const TArray<AActor*>& DeliveryTargets,
		int32 RelicPrice,
		bool bNotifyBlueprint);

	TSet<TWeakObjectPtr<ANPBaseRelic>> OverlappingRelics;
	TSet<TWeakObjectPtr<ANPBaseRelic>> DeliveryAttemptsInProgress;
	bool bDeliveryEffectEnabled = false;
};
