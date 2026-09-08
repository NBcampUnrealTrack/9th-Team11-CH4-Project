#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Gameplay/MapEvents/NPMapEvent.h"
#include "NPPossessionMapEvent.generated.h"

class ANPGhostFollowerActor;
class ANPGhostPatrolRoute;
class ANPRelicCase;
class ANPStablePhysicsPawn;
class UAbilitySystemComponent;

/** 등 뒤 유령, 서버 GAS 이동 반전, 이벤트 동안의 진열장 임시 해제를 관리합니다. */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPPossessionMapEvent : public ANPMapEvent
{
	GENERATED_BODY()

public:
	ANPPossessionMapEvent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 서버 Roaming 유령이 플레이어와 접촉했을 때 해당 플레이어에게만 빙의를 적용합니다. */
	void HandleRoamingGhostContact(ANPGhostFollowerActor* Ghost, ANPStablePhysicsPawn* PlayerPawn);

	/** 이미 빙의 중이지 않은 유효한 플레이어인지 서버 추격 고스트가 확인할 때 사용합니다. */
	bool CanRoamingGhostTarget(const ANPStablePhysicsPawn* PlayerPawn) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void ApplyEventState_Implementation(bool bNewActive) override;

	/** RoamingGhostClass가 비어 있을 때 사용할 순찰 유령 BP입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Possession Event")
	TSubclassOf<ANPGhostFollowerActor> GhostClass;

	/** 수평면 이동 전체(W/S, A/D)를 반전합니다. 시점과 점프는 유지합니다. 기존 BP 설정명은 보존합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Possession Event")
	bool bReverseHorizontalInput = true;

	/** true이면 이벤트 시작 즉시 전원에게 적용하는 기존 동작입니다. false이면 Roaming 유령 접촉 대상에게만 적용합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Possession Event")
	bool bEnableGhostAndControlEffects = false;

	/** 이벤트 시작 시 서버에서 순찰 루트에 생성할 고스트 BP입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Possession Event|Roaming Ghost")
	TSubclassOf<ANPGhostFollowerActor> RoamingGhostClass;

	/** 같은 그룹인 Ghost Patrol Route 중 하나를 서버에서 선택합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Possession Event|Roaming Ghost")
	FGameplayTag RoamingGhostRouteGroup;

	/** 이벤트에 동시에 존재할 전체 고스트 수입니다. 빙의 중인 고스트도 이 수에 포함합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Possession Event|Roaming Ghost",
		meta=(ClampMin="1", ClampMax="50", UIMin="1", UIMax="20"))
	int32 RoamingGhostCount = 3;

	/** 스트리밍 등으로 루트를 아직 찾지 못했을 때 다시 생성할 간격입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Possession Event|Roaming Ghost", meta=(ClampMin="0.1", Units="s"))
	float PatrolSpawnRetryInterval = 2.0f;

	/** 플레이어와 접촉한 순간부터 GameplayCue 연출과 입력 반전을 유지할 시간입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Possession Event|Roaming Ghost", meta=(ClampMin="0.1", Units="s"))
	float PossessionDuration = 5.0f;

	/** 빙의가 끝나 루트에서 다시 출발할 때 접촉을 무시할 시간입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Possession Event|Roaming Ghost", meta=(ClampMin="0.0", Units="s"))
	float PostPossessionContactDelay = 1.5f;

	/** 서버 대상 목록과 효과 갱신 주기입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Possession Event", meta=(ClampMin="0.05", Units="s"))
	float PlayerRefreshInterval = 0.2f;

private:
	void UpdateTrackingState();
	void RefreshPlayersAndGhosts();
	void RefreshLocalGhosts();
	void ClearLocalGhosts(bool bImmediately = false);
	void RefreshAppliedEffects();
	void RemoveAppliedEffects();
	void RefreshRelicCases();
	void RemoveTemporaryCaseUnlocks();
	void SpawnRoamingGhostsToCount(float ContactDelay = 0.0f);
	bool SpawnRoamingGhost(float ContactDelay = 0.0f);
	void ScheduleRoamingSpawnRetry();
	void FinishPossession(TWeakObjectPtr<ANPStablePhysicsPawn> PlayerKey);
	void ClearPossessionTimers();
	ANPGhostPatrolRoute* FindAvailablePatrolRoute() const;
	void DestroyRoamingGhosts();

	UFUNCTION()
	void HandleRoamingGhostDestroyed(AActor* DestroyedActor);

	UFUNCTION()
	void OnRep_AffectedPlayers();

	/** 서버가 선정한 빙의 대상입니다. 클라이언트는 이 목록으로 등 뒤 Follower를 표시합니다. */
	UPROPERTY(ReplicatedUsing=OnRep_AffectedPlayers)
	TArray<TObjectPtr<ANPStablePhysicsPawn>> AffectedPlayers;

	TMap<TWeakObjectPtr<ANPStablePhysicsPawn>, TWeakObjectPtr<ANPGhostFollowerActor>> LocalGhosts;
	TMap<TWeakObjectPtr<UAbilitySystemComponent>, FActiveGameplayEffectHandle> AppliedEffects;
	TMap<TWeakObjectPtr<ANPStablePhysicsPawn>, FTimerHandle> PossessionTimers;
	TSet<TWeakObjectPtr<ANPRelicCase>> TemporarilyUnlockedCases;
	FTimerHandle PlayerRefreshTimer;
	FTimerHandle RoamingSpawnRetryTimer;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ANPGhostFollowerActor>> SpawnedRoamingGhosts;

	bool bWarnedMissingGhostClass = false;
	bool bWarnedSpawnFailure = false;
	bool bWarnedUnsupportedPawn = false;
};
