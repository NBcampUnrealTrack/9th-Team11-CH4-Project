#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NPSquareEntranceGimmick.generated.h"

class ANPBaseRelic;
class AStaticMeshActor;
class UArrowComponent;
class UBoxComponent;
class UGrabbableComponent;
class UAudioComponent;
class USoundAttenuation;
class USoundBase;
class UStaticMeshComponent;

UENUM()
enum class ENPSquareEntrancePhase : uint8
{
	Idle,
	Falling,
	Stacked,
	Disappearing
};

USTRUCT()
struct FNPSquareEntranceState
{
	GENERATED_BODY()

	UPROPERTY()
	ENPSquareEntrancePhase Phase = ENPSquareEntrancePhase::Idle;

	UPROPERTY()
	float StartTime = 0.0f;
};

UCLASS()
class NOPHOTOS_API ANPSquareEntranceGimmick : public AActor
{
	GENERATED_BODY()

public:
	ANPSquareEntranceGimmick();
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Entrance")
	TObjectPtr<UBoxComponent> EntranceBarrier;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Entrance")
	TObjectPtr<UBoxComponent> FallingAreaBarrier;

	// 화살표의 수평 방향이 플레이어를 내보낼 입구 바깥 방향이다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Entrance")
	TObjectPtr<UArrowComponent> PushOutDirection;

	UPROPERTY(EditInstanceOnly, Category="Entrance")
	TObjectPtr<ANPBaseRelic> GoldenApple;

	// 완성 위치에 배치한 블럭을 아래쪽부터 등록한다. 이동 복제는 끈다.
	UPROPERTY(EditInstanceOnly, Category="Entrance")
	TArray<TObjectPtr<AStaticMeshActor>> Blocks;

	UPROPERTY(EditAnywhere, Category="Entrance", meta=(ClampMin="0.0", Units="cm"))
	float FallHeight = 1500.0f;

	UPROPERTY(EditAnywhere, Category="Entrance", meta=(ClampMin="0.01", Units="s"))
	float FallDuration = 0.7f;

	UPROPERTY(EditAnywhere, Category="Entrance", meta=(ClampMin="0.0", Units="s"))
	float FallInterval = 0.15f;

	UPROPERTY(EditAnywhere, Category="Entrance", meta=(ClampMin="0.01", Units="s"))
	float DisappearDuration = 0.6f;

	UPROPERTY(EditAnywhere, Category="Entrance", meta=(ClampMin="0.0", Units="s"))
	float DisappearInterval = 0.1f;

	UPROPERTY(EditAnywhere, Category="Entrance", meta=(ClampMin="1.0", Units="cm"))
	float PushOutPadding = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Entrance|Sound")
	TObjectPtr<USoundBase> GimmickSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Entrance|Sound")
	TObjectPtr<USoundAttenuation> GimmickSoundAttenuation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Entrance|Sound", meta=(ClampMin="1"))
	int32 SoundPlayCount = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Entrance|Sound", meta=(ClampMin="0.0", Units="s"))
	float SoundPlayInterval = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Entrance|Sound", meta=(ClampMin="0.01", Units="s"))
	float SoundPlayDuration = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Entrance|Sound", meta=(ClampMin="0.01"))
	float MinSoundPitch = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Entrance|Sound", meta=(ClampMin="0.01"))
	float MaxSoundPitch = 1.1f;

private:
	void HandleGrabCountChanged(int32 Count);
	void SetPhase(ENPSquareEntrancePhase Phase);
	void ApplyState();
	void StartSoundSequence();
	void PlayNextSound();
	void StopSoundSequence();
	bool PushPlayersOutsideBarriers();
	float GetServerTime() const;
	float GetSequenceDuration(bool bFalling) const;

	UFUNCTION()
	void OnRep_State();

	UPROPERTY(ReplicatedUsing=OnRep_State)
	FNPSquareEntranceState State;

	TWeakObjectPtr<UGrabbableComponent> Grabbable;
	TArray<TWeakObjectPtr<UAudioComponent>> ActiveSoundComponents;
	TArray<FTransform> FinalTransforms;
	FTimerHandle SoundSequenceTimerHandle;
	ENPSquareEntrancePhase AppliedSoundPhase = ENPSquareEntrancePhase::Idle;
	int32 PlayedSoundCount = 0;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> DisappearVisuals;
	bool bInitialized = false;
};
