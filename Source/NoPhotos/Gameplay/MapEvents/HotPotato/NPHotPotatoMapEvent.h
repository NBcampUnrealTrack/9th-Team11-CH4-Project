#pragma once

#include "ActiveGameplayEffectHandle.h"
#include "CoreMinimal.h"
#include "Gameplay/MapEvents/NPMapEvent.h"
#include "NPHotPotatoMapEvent.generated.h"

class ANPPlayerState;
class ANPHotPotatoBomb;
class ANPReplicatedStablePhysicsPawn;
class UAbilitySystemComponent;
class UGameplayEffect;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FNPHotPotatoHolderChanged,
	ANPPlayerState*,
	CurrentHolder);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FNPHotPotatoScorePenaltyApplied,
	ANPPlayerState*,
	PenalizedPlayer,
	int32,
	PenaltyScore,
	bool,
	bExplosionPenalty);

/**
 * 이벤트 시작 시점의 순위 순서대로 각 라운드의 최초 폭탄 소유자를 정합니다.
 * 라운드 중 폭탄이 전달되어도 퓨즈는 초기화하지 않으며, 폭발 후 다음 순위부터 새 라운드를 시작합니다.
 */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPHotPotatoMapEvent : public ANPMapEvent
{
	GENERATED_BODY()

public:
	ANPHotPotatoMapEvent();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Hot Potato Event")
	ANPPlayerState* GetCurrentBombHolder() const { return CurrentBombHolder; }

	UFUNCTION(BlueprintPure, Category="Hot Potato Event")
	ANPHotPotatoBomb* GetSpawnedBombActor() const { return SpawnedBombActor; }

	UFUNCTION(BlueprintPure, Category="Hot Potato Event|Timing")
	float GetRemainingBombTime() const;

	/** 다음 단계의 플레이어 잡기 판정에서 호출합니다. 퓨즈 시간은 유지됩니다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Hot Potato Event")
	bool SetCurrentBombHolder(ANPPlayerState* NewHolder);

	UPROPERTY(BlueprintAssignable, Category="Hot Potato Event")
	FNPHotPotatoHolderChanged OnBombHolderChanged;

	UPROPERTY(BlueprintAssignable, Category="Hot Potato Event")
	FNPHotPotatoScorePenaltyApplied OnScorePenaltyApplied;

protected:
	virtual void ApplyEventState_Implementation(bool bNewActive) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 소유자의 손에 부착할 폭탄 BP입니다. 자체 퓨즈가 없는 연출용 Actor를 사용합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hot Potato Event|Bomb")
	TSubclassOf<ANPHotPotatoBomb> BombActorClass;

	/** 현재 폭탄 소유자에게 적용할 상태이상 면역 효과입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hot Potato Event|Carrier")
	TSubclassOf<UGameplayEffect> CarrierImmunityEffectClass;

	/** 캐릭터의 Skeletal Mesh에 폭탄을 부착할 소켓 또는 본 이름입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hot Potato Event|Bomb")
	FName BombAttachSocketName = TEXT("hand_r");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hot Potato Event|Bomb")
	FTransform BombRelativeTransform = FTransform::Identity;

	/** 폭탄 한 라운드의 최소 퓨즈 시간입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hot Potato Event|Round",
		meta=(ClampMin="0.1", Units="s"))
	float MinimumFuseDuration = 20.0f;

	/** 폭탄 한 라운드의 최대 퓨즈 시간입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hot Potato Event|Round",
		meta=(ClampMin="0.1", Units="s"))
	float MaximumFuseDuration = 30.0f;

	/** 폭발 후 다음 순위 플레이어에게 새 폭탄을 지급하기까지의 시간입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hot Potato Event|Round",
		meta=(ClampMin="0.0", Units="s"))
	float DelayBetweenRounds = 1.0f;

	/** 이벤트 시작 시 순위 목록에서 사용할 최대 인원입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hot Potato Event|Round",
		meta=(ClampMin="1", ClampMax="6", UIMin="1", UIMax="6"))
	int32 MaximumParticipants = 6;

	/** 폭탄을 들고 있는 동안 매초 현재 점수에서 감소할 비율입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hot Potato Event|Penalty",
		meta=(ClampMin="0.0", ClampMax="100.0", UIMin="0.0", UIMax="100.0", Units="Percent"))
	float ScorePenaltyPercentPerSecond = 1.0f;

	/** 폭탄이 터질 때 현재 점수에서 감소할 비율입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hot Potato Event|Penalty",
		meta=(ClampMin="0.0", ClampMax="100.0", UIMin="0.0", UIMax="100.0", Units="Percent"))
	float ExplosionScorePenaltyPercent = 20.0f;

private:
	void BuildParticipantOrder();
	void StartNextRound();
	ANPPlayerState* FindNextRoundStarter();
	void HandleScorePenaltyTick();
	void HandleBombFuseExpired();
	void ScheduleNextRound();
	int32 ApplyPercentagePenalty(
		ANPPlayerState* PlayerState,
		float PenaltyPercent,
		bool bExplosionPenalty);
	bool IsEligibleParticipant(const ANPPlayerState* PlayerState) const;
	bool SetCurrentBombHolderInternal(ANPPlayerState* NewHolder);
	void SpawnBombForCurrentHolder();
	void AttachBombToCurrentHolder();
	void DestroySpawnedBomb();
	void ClearRoundTimers();
	void ApplyCarrierImmunity();
	void RemoveCarrierImmunity();
	void BindToCurrentCarrierGrab();
	void UnbindFromCurrentCarrierGrab();
	void HandleCarrierGrabbedPlayer(ANPReplicatedStablePhysicsPawn* GrabbedPawn);

	UFUNCTION()
	void OnRep_CurrentBombHolder();

	UPROPERTY(Transient)
	TArray<TObjectPtr<ANPPlayerState>> ParticipantOrder;

	UPROPERTY(ReplicatedUsing=OnRep_CurrentBombHolder)
	TObjectPtr<ANPPlayerState> CurrentBombHolder;

	UPROPERTY(Replicated)
	TObjectPtr<ANPHotPotatoBomb> SpawnedBombActor;

	/** GameState의 서버 동기화 시간 기준 현재 폭탄의 폭발 시각입니다. */
	UPROPERTY(Replicated)
	float BombExplosionServerWorldTime = 0.0f;

	int32 NextRoundStarterIndex = 0;
	TWeakObjectPtr<UAbilitySystemComponent> CarrierAbilitySystem;
	TWeakObjectPtr<ANPReplicatedStablePhysicsPawn> BoundCarrierPawn;
	FActiveGameplayEffectHandle CarrierImmunityEffectHandle;
	FTimerHandle BombFuseTimer;
	FTimerHandle ScorePenaltyTimer;
	FTimerHandle NextRoundTimer;
};
