#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NPStairTrapController.generated.h"

class ANPStairTrapBase;
enum class ENPStairTrapState : uint8;

/** 컨트롤러가 제어할 함정과 해당 함정의 시작 지연을 한 항목으로 묶습니다. */
USTRUCT(BlueprintType)
struct NOPHOTOS_API FNPStairTrapControlEntry
{
	GENERATED_BODY()

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Stair Trap")
	TObjectPtr<ANPStairTrapBase> Trap = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stair Trap",
		meta=(ClampMin="0.0", Units="s"))
	float StartDelay = 0.0f;
};

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
	virtual void PostLoad() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 이 컨트롤러가 제어할 함정과 각 함정의 시작 지연입니다. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Stair Trap")
	TArray<FNPStairTrapControlEntry> ControlledTrapEntries;

	/** 기존 배치 데이터의 자동 이전에만 사용합니다. */
	UPROPERTY(meta=(DeprecatedProperty,
		DeprecationMessage="Use ControlledTrapEntries instead."))
	TArray<TObjectPtr<ANPStairTrapBase>> ControlledTraps;

	/** 기존 배치 데이터의 자동 이전에만 사용합니다. */
	UPROPERTY(meta=(DeprecatedProperty,
		DeprecationMessage="Use ControlledTrapEntries instead."))
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
	void MigrateLegacyTrapEntries();
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
	float GetServerTimeSeconds() const;

	FTimerHandle PhaseTimer;
	TArray<FTimerHandle> TrapTransitionTimers;
	int32 CurrentCycleSequence = 0;
	int32 CurrentRunGeneration = 0;
	bool bContinuousTrapsStarted = false;
	bool bSequenceRunning = false;
};
