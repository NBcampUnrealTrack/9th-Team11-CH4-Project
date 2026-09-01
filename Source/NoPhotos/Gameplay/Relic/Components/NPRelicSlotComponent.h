#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Engine/DataTable.h"
#include "NPRelicSlotComponent.generated.h"

class ANPBaseRelic;
class FLifetimeProperty;

UCLASS(ClassGroup = (Relic), meta = (BlueprintSpawnableComponent))
class NOPHOTOS_API UNPRelicSlotComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UNPRelicSlotComponent();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Relic Slot")
	ANPBaseRelic* GetSpawnedRelic() const { return SpawnedRelic; }

	/** 기존 유물을 제거하고 이 슬롯에 지정된 유물을 에디터 레벨에 생성합니다. */
	UFUNCTION(CallInEditor, Category = "Relic Slot|Editor", meta = (DisplayName = "지정 유물 생성"))
	void CreateRelicInEditor();

	/** 슬롯 Transform에 유물을 생성합니다. 서버에서만 유효합니다. */
	ANPBaseRelic* SpawnRelic(bool bInitiallyReleased);

	/** 생성된 유물을 케이스 잠금에서 해제합니다. */
	void ReleaseRelic();

	/** 전시 중인 유물만 케이스 접근 상태에 맞춥니다. 이미 꺼낸 유물은 유지합니다. */
	void SetCaseAccessible(bool bAccessible);

#if WITH_EDITOR
	/** 기존 유물을 제거하고 슬롯에 지정된 유물을 에디터 레벨에 생성합니다. */
	ANPBaseRelic* RecreateRelicInEditor();
#endif

protected:
	UFUNCTION()
	void OnRep_SpawnedRelic();

	UFUNCTION()
	void OnRep_IsRelicReleased();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relic Slot", meta = (RowType = "/Script/NoPhotos.NPRelicTableRow"))
	FDataTableRowHandle RelicData;

	/** 생성 직후 케이스 잠금과 관계없이 잡을 수 있게 합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relic Slot")
	bool bInitiallyAccessible = false;

	/** 생성 직후 전시 상태를 해제하고 물리 시뮬레이션을 시작합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relic Slot")
	bool bSimulatePhysicsOnSpawn = false;

	UPROPERTY(ReplicatedUsing = OnRep_SpawnedRelic, VisibleInstanceOnly, BlueprintReadOnly, Category = "Relic Slot")
	TObjectPtr<ANPBaseRelic> SpawnedRelic;

	UPROPERTY(ReplicatedUsing = OnRep_IsRelicReleased, VisibleInstanceOnly, BlueprintReadOnly, Category = "Relic Slot")
	bool bIsRelicReleased = false;

private:
	ANPBaseRelic* CreateConfiguredRelic(EObjectFlags InObjectFlags);
	void ApplyRelicState();
};
