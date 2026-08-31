#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Goblin/NPGoblinCharacter.h"
#include "GameplayTagContainer.h"
#include "Gameplay/MapEvents/NPMapEvent.h"
#include "NPGoblinMapEvent.generated.h"

class ANPGoblinPatrolRoute;

/**
 * 이벤트 위치 레벨의 Goblin SpawnVolume에서 위치를 찾아 고블린 BP를 생성합니다.
 * 고블린의 AI와 사진 반응은 생성되는 BP가 담당합니다.
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin Event|Spawn", meta = (ClampMin = "1", UIMin = "1"))
	int32 GoblinCount = 1;

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
	void SpawnGoblins();
	ANPGoblinPatrolRoute* FindPatrolRoute() const;
	ANPGoblinCharacter* SpawnGoblinAt(
		const FTransform& GroundTransform,
		ANPGoblinPatrolRoute* PatrolRoute);
	void BeginDespawnSpawnedGoblins();
	void DestroySpawnedGoblinsImmediately();

	UPROPERTY(Transient)
	TArray<TObjectPtr<ANPGoblinCharacter>> SpawnedGoblins;
};
