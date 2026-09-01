#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Relic/NPRelicSetup.h"
#include "NPRelicRopeSetup.generated.h"

class ANPBaseRelic;
class ANPRopeAnchorActor;
class ANPRopeSegmentActor;
class FLifetimeProperty;
class USceneComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnNPRemainingRopeCountChanged,
	int32,
	RemainingRopeCount);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNPRopeAssemblyReady);

/** 핀 액터의 모든 RelicGimmick이 완료되면 연결된 Rope의 끝을 해제합니다. */
USTRUCT(BlueprintType)
struct NOPHOTOS_API FNPRelicRopeBinding
{
	GENERATED_BODY()

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Relic Rope")
	TObjectPtr<AActor> PinActor = nullptr;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Relic Rope")
	TObjectPtr<ANPRopeSegmentActor> RopeSegment = nullptr;
};

/**
 * 로프로 고정된 유물을 위한 전용 Setup입니다.
 * 핀 기믹 완료, Rope 해제, 남은 Rope 수, 유물 잠금을 함께 조정합니다.
 */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPRelicRopeSetup : public ANPRelicSetup
{
	GENERATED_BODY()

public:
	ANPRelicRopeSetup();
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Relic Rope Setup")
	int32 GetRemainingRopeCount() const { return RemainingRopeCount; }

	UFUNCTION(BlueprintPure, Category="Relic Rope Setup")
	bool IsRopeAssemblyReady() const;

	UPROPERTY(BlueprintAssignable, Category="Relic Rope Setup")
	FOnNPRemainingRopeCountChanged OnRemainingRopeCountChanged;

	/** 서버의 자동 생성 결과가 클라이언트에 준비되었을 때 발생합니다. */
	UPROPERTY(BlueprintAssignable, Category="Relic Rope Setup")
	FOnNPRopeAssemblyReady OnRopeAssemblyReady;

protected:
	virtual bool PrepareRelicSetup() override;
	virtual void CollectGimmicks() override;
	virtual void RefreshRelicLock() override;

	/** true이면 Blueprint의 SpawnPoint 컴포넌트를 읽어 유물 세트를 자동 생성합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Relic Rope Setup|Spawn")
	bool bSpawnAssemblyOnBeginPlay = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Relic Rope Setup|Spawn")
	TSubclassOf<ANPBaseRelic> RelicClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Relic Rope Setup|Spawn")
	TSubclassOf<AActor> PinClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Relic Rope Setup|Spawn")
	TSubclassOf<ANPRopeSegmentActor> RopeClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Relic Rope Setup|Spawn")
	TSubclassOf<ANPRopeAnchorActor> RopeAnchorClass;

	/** RopeWrapSpawnPoint에 생성할, 유물 둘레를 감싼 로프 표현용 Actor입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Relic Rope Setup|Spawn")
	TSubclassOf<AActor> RopeWrapVisualClass;

	/** 생성된 Pin의 Rope 끝이 붙을 컴포넌트 태그입니다. 비어 있으면 RootComponent를 사용합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Relic Rope Setup|Spawn")
	FName PinRopeComponentTag = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Relic Rope Setup|Spawn")
	FName PinRopeSocketName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Relic Rope Setup|Spawn")
	FVector PinRopeLocalOffset = FVector::ZeroVector;

	/** 모든 핀이 완료된 뒤 Rope 묶음 전체가 이동할 월드 오프셋입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Relic Rope Setup|Release")
	FVector RopeReleaseWorldOffset = FVector(0.0f, 0.0f, -100.0f);

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category="Relic Rope Setup|Release",
		meta=(ClampMin="0.0"))
	float RopeReleaseDuration = 1.25f;

	/** 마지막 핀 완료 후 Rope 하강과 Relic 물리를 시작하기 전 대기 시간입니다. */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category="Relic Rope Setup|Release",
		meta=(ClampMin="0.0"))
	float RopeReleaseDelay = 1.0f;

	/** true이면 고정 오프셋의 Z 대신 아래쪽 바닥을 찾아 둘레 Rope 비주얼을 내립니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Relic Rope Setup|Release")
	bool bReleaseRopeWrapToGround = true;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category="Relic Rope Setup|Release",
		meta=(ClampMin="0.0"))
	float RopeReleaseGroundTraceDistance = 2000.0f;

	/** RopeWrap 비주얼의 피벗이 바닥에서 멈출 높이입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Relic Rope Setup|Release")
	float RopeReleaseGroundClearance = 5.0f;

	/** 배열 크기가 연결된 전체 Rope 수이며 PinActor는 기믹 목록에 자동 포함됩니다. */
	UPROPERTY(
		EditInstanceOnly,
		ReplicatedUsing=OnRep_RopeAssembly,
		BlueprintReadOnly,
		Category="Relic Rope Setup")
	TArray<FNPRelicRopeBinding> RopeBindings;

private:
	bool SpawnAssemblyFromMarkers();
	USceneComponent* FindMarker(const FName& MarkerName) const;
	void CollectIndexedMarkers(
		const FString& Prefix,
		TMap<FString, USceneComponent*>& OutMarkers) const;
	void RefreshRopeBindings();
	void ScheduleRopeRelease();
	void StartRopeRelease();
	void ReleaseRelicThroughGrabContract();
	void UpdateRopeRelease(float DeltaSeconds);
	FVector ResolveRopeReleaseTargetLocation() const;
	bool IsRopeBindingCompleted(const FNPRelicRopeBinding& Binding) const;

	UFUNCTION()
	void OnRep_RemainingRopeCount();

	UFUNCTION()
	void OnRep_RopeAssembly();
	void BroadcastRopeAssemblyReady();

	UPROPERTY(Transient, VisibleInstanceOnly, Category="Relic Rope Setup|Debug")
	TArray<TObjectPtr<AActor>> SpawnedPins;

	UPROPERTY(Transient, VisibleInstanceOnly, Category="Relic Rope Setup|Debug")
	TArray<TObjectPtr<ANPRopeSegmentActor>> SpawnedRopes;

	UPROPERTY(Transient, VisibleInstanceOnly, Category="Relic Rope Setup|Debug")
	TArray<TObjectPtr<ANPRopeAnchorActor>> SpawnedAnchors;

	/** 둘러진 Rope 비주얼과 모든 RopeAnchor가 함께 내려가는 공통 루트입니다. */
	UPROPERTY(Transient, VisibleInstanceOnly, Category="Relic Rope Setup|Debug")
	TObjectPtr<ANPRopeAnchorActor> SpawnedRopeReleaseRoot;

	UPROPERTY(Transient, VisibleInstanceOnly, Category="Relic Rope Setup|Debug")
	TObjectPtr<AActor> SpawnedRopeWrapVisual;

	FVector RopeReleaseStartLocation = FVector::ZeroVector;
	FVector RopeReleaseTargetLocation = FVector::ZeroVector;
	float RopeReleaseElapsedTime = 0.0f;
	FTimerHandle RopeReleaseDelayTimer;
	bool bRopeReleaseScheduled = false;
	bool bRopeReleaseStarted = false;
	bool bRopeAssemblyReadyNotificationPending = false;
	bool bHasBroadcastRopeAssemblyReady = false;

	UPROPERTY(
		ReplicatedUsing=OnRep_RemainingRopeCount,
		VisibleInstanceOnly,
		BlueprintReadOnly,
		Category="Relic Rope Setup|State",
		meta=(AllowPrivateAccess="true"))
	int32 RemainingRopeCount = 0;
};
