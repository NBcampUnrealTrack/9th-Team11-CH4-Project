#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "Gameplay/MapEvents/NPMapEvent.h"
#include "NPPossessionMapEvent.generated.h"

class ANPGhostFollowerActor;
class ANPRelicCase;
class ANPStablePhysicsPawn;
class UAbilitySystemComponent;

/** 등 뒤 유령, 서버 GAS 이동 반전, 이벤트 동안의 진열장 임시 해제를 관리합니다. */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPPossessionMapEvent : public ANPMapEvent
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void ApplyEventState_Implementation(bool bNewActive) override;

	/** NPGhostFollowerActor 자식 BP에 외형을 지정한 후 연결합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Possession Event")
	TSubclassOf<ANPGhostFollowerActor> GhostClass;

	/** 수평면 이동 전체(W/S, A/D)를 반전합니다. 시점과 점프는 유지합니다. 기존 BP 설정명은 보존합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Possession Event")
	bool bReverseHorizontalInput = true;

	/** 서버 대상 목록과 로컬 표시 갱신 주기. 중도 접속/리스폰/복제 지연도 함께 처리합니다. */
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

	UFUNCTION()
	void OnRep_AffectedPlayers();

	/** 클라이언트에는 타인의 PlayerController가 없으므로 서버가 선정한 Pawn 목록을 복제합니다. */
	UPROPERTY(ReplicatedUsing=OnRep_AffectedPlayers)
	TArray<TObjectPtr<ANPStablePhysicsPawn>> AffectedPlayers;

	TMap<TWeakObjectPtr<ANPStablePhysicsPawn>, TWeakObjectPtr<ANPGhostFollowerActor>> LocalGhosts;
	TMap<TWeakObjectPtr<UAbilitySystemComponent>, FActiveGameplayEffectHandle> AppliedEffects;
	TSet<TWeakObjectPtr<ANPRelicCase>> TemporarilyUnlockedCases;
	FTimerHandle PlayerRefreshTimer;
	bool bWarnedMissingGhostClass = false;
	bool bWarnedSpawnFailure = false;
	bool bWarnedUnsupportedPawn = false;
};
