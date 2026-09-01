#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Gameplay/MapEvents/NPMapEvent.h"
#include "NPRelicBonusMapEvent.generated.h"

class ANPRelicReturnZone;
class ANPRelicBonusCountdownActor;
class ANPRopeAnchorActor;
class ANPRopeSegmentActor;
class UCurveFloat;
class UNiagaraComponent;
class UNiagaraSystem;
class USceneComponent;

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

	UFUNCTION(BlueprintPure, Category = "Relic Bonus Event")
	int32 GetSpawnedRopeCount() const { return SpawnedRopes.Num(); }

protected:
	virtual void Tick(float DeltaSeconds) override;
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

	/** 이벤트 연출에 사용할 RopeSegment BP 클래스입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic Bonus Event|Rope")
	TSubclassOf<ANPRopeSegmentActor> RopeClass;

	/** 로프 끝을 아래로 이동시키는 가벼운 복제 Actor 클래스입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic Bonus Event|Rope")
	TSubclassOf<ANPRopeAnchorActor> RopeTipClass;

	/** 헬리콥터 BP에서 로프가 시작될 SceneComponent에 지정할 Component Tag입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic Bonus Event|Rope")
	FName RopeStartComponentTag = TEXT("RopeStart");

	/** 반환 존 BP에서 로프가 도착할 SceneComponent에 지정할 Component Tag입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic Bonus Event|Rope")
	FName RopeEndComponentTag = TEXT("RopeEnd");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic Bonus Event|Rope", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float RopeLoweringDuration = 3.0f;

	/** 지정하지 않으면 Ease In/Out 보간을 사용합니다. X=진행도(0~1), Y=이동 비율을 권장합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic Bonus Event|Rope")
	TObjectPtr<UCurveFloat> RopeLoweringCurve;

	/** RopeEnd 컴포넌트가 없을 때 지면 기준 도착 높이입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic Bonus Event|Rope", meta = (Units = "cm"))
	float RopeEndHeightOffset = 0.0f;

	/** 이벤트 종료 후 운반체가 사라지기까지 상승하는 시간입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic Bonus Event|Departure", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float DepartureDuration = 4.0f;

	/** 이벤트 종료 위치에서 추가로 상승할 Z 높이입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic Bonus Event|Departure", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float DepartureHeight = 2500.0f;

	/** 지정하지 않으면 Ease In 보간을 사용합니다. X=진행도(0~1), Y=이동 비율을 권장합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic Bonus Event|Departure")
	TObjectPtr<UCurveFloat> DepartureCurve;

	/** 운반체가 도착했을 때 지면에 생성할 지속형 바람 Niagara System입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic Bonus Event|Ground Wind")
	TSoftObjectPtr<UNiagaraSystem> GroundWindSystem;

	/** 지면 끼임을 막기 위해 Niagara를 지면에서 띄울 높이입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic Bonus Event|Ground Wind", meta = (Units = "cm"))
	float GroundWindHeightOffset = 10.0f;

private:
	struct FActiveRopeDeployment
	{
		TWeakObjectPtr<ANPRopeAnchorActor> RopeTip;
		FVector StartLocation = FVector::ZeroVector;
		FVector TargetLocation = FVector::ZeroVector;
		float ElapsedTime = 0.0f;
	};

	struct FActiveDeparture
	{
		TWeakObjectPtr<AActor> Carrier;
		FVector StartLocation = FVector::ZeroVector;
		FVector TargetLocation = FVector::ZeroVector;
		float ElapsedTime = 0.0f;
	};

	void SpawnReturnZones();
	ANPRelicReturnZone* SpawnReturnZoneAt(const FTransform& GroundTransform);
	ANPRelicBonusCountdownActor* SpawnCountdownAt(const FTransform& GroundTransform);
	AActor* SpawnHelicopterAt(const FTransform& GroundTransform);
	bool BeginRopeDeployment(
		AActor* Helicopter,
		ANPRelicReturnZone* ReturnZone,
		const FTransform& GroundTransform);
	void BeginDeparture();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastSpawnGroundWind(FVector GroundLocation);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastFadeGroundWind();

	void StopGroundWindImmediately();
	USceneComponent* FindTaggedSceneComponent(AActor* Actor, FName ComponentTag) const;
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

	UPROPERTY(Transient)
	TArray<TObjectPtr<ANPRopeSegmentActor>> SpawnedRopes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ANPRopeAnchorActor>> SpawnedRopeTips;

	/** 각 머신에 로컬로 생성된 Niagara 컴포넌트입니다. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraComponent>> GroundWindComponents;

	TArray<FActiveRopeDeployment> ActiveRopeDeployments;
	TArray<FActiveDeparture> ActiveDepartures;
};
