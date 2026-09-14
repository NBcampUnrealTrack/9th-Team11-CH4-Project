#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NPStairTrapBase.generated.h"

class ANPStairTrapController;
class USceneComponent;

UENUM(BlueprintType)
enum class ENPStairTrapState : uint8
{
	Disabled,
	Idle,
	Warning,
	Active,
	Returning,
	Cooldown
};

/** 컨트롤러가 함정을 반복 작동시킬지, 한 번 켜서 계속 유지할지 결정합니다. */
UENUM(BlueprintType)
enum class ENPStairTrapOperationMode : uint8
{
	Cyclic,
	Continuous
};

/** 클라이언트가 하나의 일관된 함정 단계 정보를 받도록 묶은 복제 상태입니다. */
USTRUCT(BlueprintType)
struct NOPHOTOS_API FNPStairTrapRepState
{
	GENERATED_BODY()

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Stair Trap")
	ENPStairTrapState State = ENPStairTrapState::Idle;

	/** 이 단계가 시작된 서버 월드 시간입니다. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Stair Trap")
	float PhaseStartServerTime = 0.0f;

	/** 같은 단계가 반복되어도 새로운 작동임을 구분하는 번호입니다. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Stair Trap")
	int32 CycleSequence = 0;
};

/**
 * 계단 함정의 공통 네트워크 상태와 컨트롤러 연결 지점입니다.
 * 실제 창 이동, 진자 운동, 충돌 및 넉백은 파생 클래스에서 구현합니다.
 */
UCLASS(Abstract, Blueprintable)
class NOPHOTOS_API ANPStairTrapBase : public AActor
{
	GENERATED_BODY()

public:
	ANPStairTrapBase();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Stair Trap")
	ENPStairTrapState GetTrapState() const { return ReplicatedTrapState.State; }

	UFUNCTION(BlueprintPure, Category="Stair Trap")
	int32 GetTrapCycleSequence() const
	{
		return ReplicatedTrapState.CycleSequence;
	}

	/** 현재 단계가 시작된 뒤 서버 시간 기준으로 흐른 시간입니다. */
	UFUNCTION(BlueprintPure, Category="Stair Trap")
	float GetTrapPhaseElapsedTime() const;

	UFUNCTION(BlueprintPure, Category="Stair Trap")
	bool IsTrapActive() const
	{
		return ReplicatedTrapState.State == ENPStairTrapState::Active;
	}

	UFUNCTION(BlueprintPure, Category="Stair Trap")
	ENPStairTrapOperationMode GetTrapOperationMode() const
	{
		return OperationMode;
	}

	/** 서버에서 이 함정을 수동으로 활성/비활성화할 때 사용합니다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Stair Trap")
	void SetTrapEnabled(bool bEnabled);

protected:
	/** Cyclic은 단계별 반복, Continuous는 시작 지연 후 계속 Active 상태를 유지합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stair Trap")
	ENPStairTrapOperationMode OperationMode =
		ENPStairTrapOperationMode::Cyclic;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stair Trap")
	TObjectPtr<USceneComponent> TrapRootComponent;

	/** 서버와 모든 클라이언트에서 단계가 바뀔 때 한 번 호출됩니다. */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="Stair Trap",
		meta=(DisplayName="On Trap State Changed"))
	void BP_OnTrapStateChanged(
		ENPStairTrapState PreviousState,
		ENPStairTrapState NewState,
		int32 CycleSequence,
		float PhaseStartServerTime);

	/** 파생 C++ 클래스가 상태 변화에 맞춰 충돌 등을 제어할 수 있는 지점입니다. */
	virtual void HandleTrapStateChanged(
		ENPStairTrapState PreviousState,
		ENPStairTrapState NewState);

private:
	friend class ANPStairTrapController;

	void ApplyControlledState(
		ENPStairTrapState NewState,
		float PhaseStartServerTime,
		int32 CycleSequence);
	void NotifyTrapStateChanged(ENPStairTrapState PreviousState);

	UFUNCTION()
	void OnRep_TrapState(FNPStairTrapRepState PreviousState);

	UPROPERTY(ReplicatedUsing=OnRep_TrapState, VisibleInstanceOnly,
		BlueprintReadOnly, Category="Stair Trap", meta=(AllowPrivateAccess="true"))
	FNPStairTrapRepState ReplicatedTrapState;
};
