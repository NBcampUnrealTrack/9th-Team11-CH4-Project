#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Gameplay/MapEvents/NPMapEvent.h"
#include "NPRelicBonusMapEvent.generated.h"

class ANPRelicReturnZone;
class ANPRelicBonusCountdownActor;
class UNiagaraComponent;
class UNiagaraSystem;

/**
 * 유물 보너스 이벤트의 생성물과 수명을 관리합니다.
 * 현재 단계에서는 유효한 임의의 지면 위치에 임시 반환 존을 생성합니다.
 */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPRelicBonusMapEvent : public ANPMapEvent
{
	GENERATED_BODY()

public:
	ANPRelicBonusMapEvent();

	UFUNCTION(BlueprintPure, Category = "Relic Bonus Event")
	int32 GetSpawnedReturnZoneCount() const { return SpawnedReturnZones.Num(); }

	UFUNCTION(BlueprintPure, Category = "Relic Bonus Event")
	int32 GetSpawnedHelicopterCount() const { return SpawnedHelicopters.Num(); }

protected:
	virtual void ApplyEventState_Implementation(bool bNewActive) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 동적으로 생성할 반환 존 BP 클래스입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic Bonus Event|Return Zone")
	TSubclassOf<ANPRelicReturnZone> ReturnZoneClass;

	/** 반환 존을 배치할 Spawn Volume 그룹입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic Bonus Event|Return Zone")
	FGameplayTag ReturnZoneSpawnGroup;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic Bonus Event|Return Zone", meta = (ClampMin = "0", UIMin = "0"))
	int32 MinimumReturnZoneCount = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic Bonus Event|Return Zone", meta = (ClampMin = "0", UIMin = "0"))
	int32 MaximumReturnZoneCount = 1;

	/** 생성된 반환 존끼리 추가로 확보해야 하는 최소 간격입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic Bonus Event|Return Zone", meta = (ClampMin = "0.0", Units = "cm"))
	float MinimumDistanceBetweenReturnZones = 300.0f;

	/** 반환 존 하나의 위치를 다시 추첨할 최대 횟수입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic Bonus Event|Return Zone", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaximumPlacementAttemptsPerZone = 10;

	/** 반환 존 위에 생성할 월드 카운트다운 액터 BP 클래스입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic Bonus Event|World UI")
	TSubclassOf<ANPRelicBonusCountdownActor> CountdownActorClass;

	/** 지면 기준 카운트다운 UI 높이입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic Bonus Event|World UI", meta = (Units = "cm"))
	float CountdownHeightOffset = 180.0f;

	/** 반환 존의 수직 상공에 생성할 헬리콥터 BP 클래스입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic Bonus Event|Helicopter")
	TSubclassOf<AActor> HelicopterClass;

	/** 지면 기준 헬리콥터의 최소 Z 높이입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic Bonus Event|Helicopter", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float MinimumHelicopterHeight = 1500.0f;

	/** 지면 기준 헬리콥터의 최대 Z 높이입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic Bonus Event|Helicopter", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float MaximumHelicopterHeight = 2000.0f;

	/** 한 사이클에서 헬리콥터가 머무르는 최소 시간입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic Bonus Event|Helicopter|Cycle", meta = (ClampMin = "0.1", UIMin = "0.1", Units = "s"))
	float MinimumHelicopterStayDuration = 10.0f;

	/** 한 사이클에서 헬리콥터가 머무르는 최대 시간입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic Bonus Event|Helicopter|Cycle", meta = (ClampMin = "0.1", UIMin = "0.1", Units = "s"))
	float MaximumHelicopterStayDuration = 20.0f;

	/** 한 사이클의 헬리콥터 생성에 실패했을 때 다시 시도할 시간입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic Bonus Event|Helicopter|Cycle", meta = (ClampMin = "0.1", UIMin = "0.1", Units = "s"))
	float CycleSpawnRetryDelay = 1.0f;

	/** 퇴장 완료 후 다음 헬리콥터 사이클까지 기다릴 최소 시간입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic Bonus Event|Helicopter|Cycle", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float MinimumCycleRespawnDelay = 2.0f;

	/** 퇴장 완료 후 다음 헬리콥터 사이클까지 기다릴 최대 시간입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic Bonus Event|Helicopter|Cycle", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float MaximumCycleRespawnDelay = 5.0f;

	/** 퇴장 신호를 보낸 뒤 헬리콥터를 실제로 제거하기까지 기다리는 시간입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic Bonus Event|Helicopter|Departure", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float HelicopterDepartureDuration = 2.0f;

	/** 운반체가 도착했을 때 지면에 생성할 지속형 바람 Niagara System입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic Bonus Event|Ground Wind")
	TSoftObjectPtr<UNiagaraSystem> GroundWindSystem;

	/** 지면 끼임을 막기 위해 Niagara를 지면에서 띄울 높이입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic Bonus Event|Ground Wind", meta = (Units = "cm"))
	float GroundWindHeightOffset = 10.0f;

private:
	void SpawnReturnZones();
	void StartNextHelicopterCycle();
	void ScheduleHelicopterDeparture();
	void HandleHelicopterStayFinished();
	void FinishDepartureCycle();
	ANPRelicReturnZone* SpawnReturnZoneAt(const FTransform& GroundTransform);
	ANPRelicBonusCountdownActor* SpawnCountdownAt(const FTransform& GroundTransform);
	AActor* SpawnHelicopterAt(const FTransform& GroundTransform);
	void BeginDeparture(bool bShouldRespawn);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastSpawnGroundWind(FVector GroundLocation);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastFadeGroundWind();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastBeginHelicopterDeparture(
		const TArray<AActor*>& Helicopters,
		float DepartureDuration);

	void StopGroundWindImmediately();
	void DestroyReturnZonesAndCountdowns();
	void DestroySpawnedActors();
	FVector GetReturnZoneHalfExtent() const;
	bool IsFarEnoughFromSpawnedZones(
		const FVector& CandidateLocation,
		const FVector& ReturnZoneHalfExtent) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ANPRelicReturnZone>> SpawnedReturnZones;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ANPRelicBonusCountdownActor>> SpawnedCountdownActors;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> SpawnedHelicopters;

	/** 각 머신에 로컬로 생성된 Niagara 컴포넌트입니다. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraComponent>> GroundWindComponents;

	FTimerHandle HelicopterStayTimer;
	FTimerHandle HelicopterDepartureTimer;
	FTimerHandle NextCycleTimer;
	bool bRespawnAfterDeparture = false;
};
