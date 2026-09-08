#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Gameplay/MapEvents/NPMapEvent.h"
#include "NPPossessionMapEvent.generated.h"

class ANPGhostFollowerActor;
class ANPRelicCase;
class ANPStablePhysicsPawn;
class UAbilitySystemComponent;

/** 추격 유령, 서버 GAS 이동 반전, 이벤트 동안의 진열장 임시 해제를 관리합니다. */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPPossessionMapEvent : public ANPMapEvent
{
	GENERATED_BODY()

public:
	ANPPossessionMapEvent();

	/** 서버 Roaming 유령이 플레이어와 접촉했을 때 해당 플레이어에게만 빙의를 적용합니다. */
	void HandleRoamingGhostContact(ANPGhostFollowerActor* Ghost, ANPStablePhysicsPawn* PlayerPawn);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void ApplyEventState_Implementation(bool bNewActive) override;

	/** RoamingGhostClass가 비어 있을 때 사용할 추격 유령 BP입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Possession Event")
	TSubclassOf<ANPGhostFollowerActor> GhostClass;

	/** 수평면 이동 전체(W/S, A/D)를 반전합니다. 시점과 점프는 유지합니다. 기존 BP 설정명은 보존합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Possession Event")
	bool bReverseHorizontalInput = true;

	/** true이면 이벤트 시작 즉시 전원에게 적용하는 기존 동작입니다. false이면 Roaming 유령 접촉 대상에게만 적용합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Possession Event")
	bool bEnableGhostAndControlEffects = false;

	/** 이벤트 시작 시 서버에서 Point에 생성할 추적 대기 상태의 고스트 BP입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Possession Event|Roaming Ghost")
	TSubclassOf<ANPGhostFollowerActor> RoamingGhostClass;

	/** 고스트를 생성할 MapEventSpawnPoint 그룹입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Possession Event|Roaming Ghost")
	FGameplayTag RoamingGhostSpawnGroup;

	/** 한 플레이어를 추격하거나 빙의 상태로 머무른 뒤 다음 대상을 선택하기까지의 시간입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Possession Event|Roaming Ghost", meta=(ClampMin="0.1", Units="s"))
	float ChaseTargetDuration = 10.0f;

	/** 플레이어와 접촉한 순간부터 입력 반전을 유지할 시간입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Possession Event|Roaming Ghost", meta=(ClampMin="0.1", Units="s"))
	float PossessionDuration = 5.0f;

	/** 빙의가 끝나 플레이어 위치에서 다시 출발할 때 접촉을 무시할 시간입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Possession Event|Roaming Ghost", meta=(ClampMin="0.0", Units="s"))
	float PostPossessionContactDelay = 1.5f;

	/** 서버 대상 목록과 효과 갱신 주기입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Possession Event", meta=(ClampMin="0.05", Units="s"))
	float PlayerRefreshInterval = 0.2f;

private:
	void UpdateTrackingState();
	void RefreshPlayersAndGhosts();
	void RefreshAppliedEffects();
	void RemoveAppliedEffects();
	void RefreshRelicCases();
	void RemoveTemporaryCaseUnlocks();
	void BeginNextChaseCycle();
	void SpawnRoamingGhost(const FTransform* OverrideTransform = nullptr, float ContactDelay = 0.0f);
	ANPStablePhysicsPawn* SelectRandomChaseTarget() const;
	void DestroyRoamingGhost();

	/** 서버가 이 이벤트의 효과를 적용할 대상입니다. 연출은 캐릭터의 GAS 태그를 사용합니다. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ANPStablePhysicsPawn>> AffectedPlayers;

	TMap<TWeakObjectPtr<UAbilitySystemComponent>, FActiveGameplayEffectHandle> AppliedEffects;
	TSet<TWeakObjectPtr<ANPRelicCase>> TemporarilyUnlockedCases;
	FTimerHandle PlayerRefreshTimer;
	FTimerHandle ChaseCycleTimer;

	UPROPERTY(Transient)
	TObjectPtr<ANPGhostFollowerActor> SpawnedRoamingGhost;

	TWeakObjectPtr<ANPStablePhysicsPawn> CurrentChaseTarget;
	TWeakObjectPtr<ANPStablePhysicsPawn> PreviousChaseTarget;

	bool bWarnedUnsupportedPawn = false;
};
