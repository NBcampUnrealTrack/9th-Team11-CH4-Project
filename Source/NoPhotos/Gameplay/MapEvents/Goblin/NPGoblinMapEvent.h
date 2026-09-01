#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Goblin/NPGoblinCharacter.h"
#include "GameplayTagContainer.h"
#include "Gameplay/MapEvents/NPMapEvent.h"
#include "NPGoblinMapEvent.generated.h"

class ANPGoblinPatrolRoute;

/**
 * 이벤트 위치 레벨의 Goblin SpawnVolume에서 위치를 찾아 고블린 BP를 생성합니다.
 * 한 번에 한 마리만 유지하며 퇴장 완료 후 남은 이벤트 시간 동안 다시 생성합니다.
 */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPGoblinMapEvent : public ANPMapEvent
{
	GENERATED_BODY()

public:
	ANPGoblinMapEvent();

	UFUNCTION(BlueprintPure, Category = "Goblin Event")
	int32 GetSpawnedGoblinCount() const { return SpawnedGoblins.Num(); }

protected:
	virtual void ApplyEventState_Implementation(bool bNewActive) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 이벤트가 시작될 때 서버에서 생성할 고블린 BP 클래스입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin Event|Spawn")
	TSubclassOf<ANPGoblinCharacter> GoblinClass;

	/** 고블린을 배치할 SpawnVolume 그룹입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin Event|Spawn")
	FGameplayTag GoblinSpawnGroup;

	/** 기존 BP 저장값 호환용입니다. 실제 동시 소환 수는 항상 1입니다. */
	UPROPERTY()
	int32 GoblinCount = 1;

	/** 퇴장 완료 후 재소환까지의 시간입니다. 이벤트 종료 시 취소됩니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin Event|Spawn", meta = (ClampMin = "0.1", Units = "s"))
	float RespawnDelay = 1.0f;

	/** 루트/스폰 볼륨/NavMesh가 준비되지 않았을 때 다시 시도할 간격입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin Event|Spawn", meta = (ClampMin = "0.5", Units = "s"))
	float SpawnRetryInterval = 2.0f;

	/** SpawnVolume이 고블린을 위해 확보해야 하는 공간의 반지름입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin Event|Spawn", meta = (ClampMin = "1.0", Units = "cm"))
	FVector GoblinRequiredHalfExtent = FVector(50.0f, 50.0f, 100.0f);

	/** FindRandomSpawnTransform은 지면 위치를 반환하므로 중앙 Pivot BP에는 Capsule Half Height를 지정합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin Event|Spawn", meta = (ClampMin = "0.0", Units = "cm"))
	float GoblinSpawnHeightOffset = 100.0f;

	/** 고블린 한 마리의 유효한 위치를 다시 찾을 최대 횟수입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin Event|Spawn", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaximumSpawnAttemptsPerGoblin = 10;

private:
	friend class FNPGoblinEventLifecycleTest;
	void TrySpawnGoblin();
	bool CanSpawnGoblin() const;
	void ScheduleSpawn(float Delay);

	UFUNCTION()
	void HandleGoblinDestroyed(AActor* DestroyedActor);
	ANPGoblinPatrolRoute* FindPatrolRoute() const;
	ANPGoblinCharacter* SpawnGoblinAt(
		const FTransform& GroundTransform,
		ANPGoblinPatrolRoute* PatrolRoute);
	void BeginDespawnSpawnedGoblins();
	void DestroySpawnedGoblinsImmediately();

	UPROPERTY(Transient)
	TArray<TObjectPtr<ANPGoblinCharacter>> SpawnedGoblins;

	FTimerHandle SpawnTimer;
	bool bAllowRespawning = false;
};
