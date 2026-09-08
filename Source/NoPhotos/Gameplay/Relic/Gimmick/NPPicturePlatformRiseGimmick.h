#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NPPicturePlatformRiseGimmick.generated.h"

class ANPBaseRelic;
class FLifetimeProperty;

USTRUCT(BlueprintType)
struct FNPPlatformRiseEntry
{
	GENERATED_BODY()

	/** 레벨에 미리 배치한 발판 액터입니다. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Platform Rise")
	TObjectPtr<AActor> PlatformActor = nullptr;

	/** 레벨에 배치된 시작 위치에서 월드 Z축으로 상승할 거리입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Platform Rise", meta=(Units="cm"))
	float RiseHeight = 100.0f;

	/** 전체 기믹이 시작된 뒤 이 발판이 움직이기 시작할 때까지의 시간입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Platform Rise", meta=(ClampMin="0.0", Units="s"))
	float StartDelay = 0.0f;

	/** 이 발판이 시작 위치에서 목표 위치까지 이동하는 시간입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Platform Rise", meta=(ClampMin="0.01", Units="s"))
	float RiseDuration = 1.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNPOnPlatformRiseEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FNPOnPlatformReachedTarget,
	int32,
	PlatformIndex);

/**
 * 지정된 그림 유물이 전시 상태에서 떨어지면 레벨에 배치된 발판들을
 * 서버 시각에 맞춰 순차적으로 상승시키는 네트워크 기믹입니다.
 */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPPicturePlatformRiseGimmick : public AActor
{
	GENERATED_BODY()

public:
	ANPPicturePlatformRiseGimmick();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Picture Platform Rise")
	bool HasRiseStarted() const { return bRiseStarted; }

	UFUNCTION(BlueprintPure, Category="Picture Platform Rise")
	bool HasRiseCompleted() const { return bRiseCompleted; }

	UPROPERTY(BlueprintAssignable, Category="Picture Platform Rise|Events")
	FNPOnPlatformRiseEvent OnPlatformRiseStarted;

	UPROPERTY(BlueprintAssignable, Category="Picture Platform Rise|Events")
	FNPOnPlatformReachedTarget OnPlatformReachedTarget;

	UPROPERTY(BlueprintAssignable, Category="Picture Platform Rise|Events")
	FNPOnPlatformRiseEvent OnPlatformRiseCompleted;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	/** 벽에서 떼어낼 그림 유물 인스턴스입니다. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Picture Platform Rise")
	TObjectPtr<ANPBaseRelic> TriggerRelic;

	/** 배열 순서와 각 항목의 지연 시간에 따라 상승할 발판들입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Picture Platform Rise")
	TArray<FNPPlatformRiseEntry> Platforms;

	/** 1보다 크면 이동의 시작과 끝이 더 부드러워집니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Picture Platform Rise", meta=(ClampMin="1.0"))
	float EaseExponent = 2.0f;

private:
	struct FPlatformRuntimeState
	{
		FVector StartLocation = FVector::ZeroVector;
		FVector TargetLocation = FVector::ZeroVector;
		bool bReachedTarget = false;
	};

	void InitializePlatforms();
	void HandleRelicReleasedFromDisplay(ANPBaseRelic* ReleasedRelic);
	void StartPlatformRise();
	void BeginLocalRisePlayback();
	void ApplyPlatformRise(float CurrentServerTime);
	void FinishPlatformRise();
	float GetServerWorldTimeSeconds() const;

	UFUNCTION()
	void OnRep_RiseStarted();

	UPROPERTY(ReplicatedUsing=OnRep_RiseStarted, VisibleInstanceOnly, Category="Picture Platform Rise")
	bool bRiseStarted = false;

	UPROPERTY(Replicated, VisibleInstanceOnly, Category="Picture Platform Rise")
	float RiseStartServerTime = 0.0f;

	TArray<FPlatformRuntimeState> PlatformRuntimeStates;
	bool bRiseCompleted = false;
	bool bRiseStartedEventBroadcast = false;
	bool bRiseCompletedEventBroadcast = false;
};
