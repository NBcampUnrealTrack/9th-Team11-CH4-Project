#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NPStairTrapController.generated.h"

class ANPStairTrapBase;
enum class ENPStairTrapState : uint8;

/** 한 층에 배치된 계단 함정들의 공통 작동 주기를 서버에서 제어합니다. */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPStairTrapController : public AActor
{
	GENERATED_BODY()

public:
	ANPStairTrapController();

	/** 레벨 인스턴스 준비가 끝난 뒤 서버에서 호출합니다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Stair Trap")
	void StartTrapSequence();

	/** 작동 주기를 멈추고 모든 함정을 Idle 상태로 되돌립니다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Stair Trap")
	void StopTrapSequence();

	UFUNCTION(BlueprintPure, Category="Stair Trap")
	bool IsTrapSequenceRunning() const { return bSequenceRunning; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 이 컨트롤러가 함께 제어할 같은 층의 함정들입니다. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Stair Trap")
	TArray<TObjectPtr<ANPStairTrapBase>> ControlledTraps;

	/**
	 * ControlledTraps와 같은 인덱스를 사용하는 함정별 시작 지연입니다.
	 * 값이 없는 인덱스는 0초로 처리합니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stair Trap|Timing",
		meta=(ClampMin="0.0"))
	TArray<float> TrapStartDelays;

	/** 테스트 맵 등에서만 사용합니다. 방 스트리밍 맵에서는 false가 안전합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stair Trap|Timing")
	bool bStartAutomatically = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stair Trap|Timing")
	bool bLoopSequence = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stair Trap|Timing",
		meta=(ClampMin="0.0", Units="s"))
	float InitialDelay = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stair Trap|Timing",
		meta=(ClampMin="0.0", Units="s"))
	float WarningDuration = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stair Trap|Timing",
		meta=(ClampMin="0.01", Units="s"))
	float ActiveDuration = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stair Trap|Timing",
		meta=(ClampMin="0.0", Units="s"))
	float ReturningDuration = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stair Trap|Timing",
		meta=(ClampMin="0.0", Units="s"))
	float CooldownDuration = 2.0f;

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="Stair Trap",
		meta=(DisplayName="On Trap Cycle Started"))
	void BP_OnTrapCycleStarted(int32 CycleSequence);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="Stair Trap",
		meta=(DisplayName="On Trap Sequence Stopped"))
	void BP_OnTrapSequenceStopped();

private:
	void BeginWarningPhase();
	void FinishCycle();
	void ApplyTrapState(
		int32 TrapIndex,
		ENPStairTrapState NewState,
		int32 CycleSequence,
		int32 RunGeneration);
	void ScheduleTrapState(
		int32 TrapIndex,
		ENPStairTrapState NewState,
		float Delay,
		int32 CycleSequence,
		int32 RunGeneration);
	void SetAllTrapStates(ENPStairTrapState NewState);
	void ScheduleTransition(FTimerDelegate Transition, float Delay);
	void ClearScheduledTransitions();
	float GetTrapStartDelay(int32 TrapIndex) const;
	float GetServerTimeSeconds() const;

	FTimerHandle PhaseTimer;
	TArray<FTimerHandle> TrapTransitionTimers;
	int32 CurrentCycleSequence = 0;
	int32 CurrentRunGeneration = 0;
	bool bContinuousTrapsStarted = false;
	bool bSequenceRunning = false;
};
