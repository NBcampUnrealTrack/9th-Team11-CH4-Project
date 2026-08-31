#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
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

#if WITH_EDITOR
	/** 기존 유물을 제거하고 슬롯에 지정된 유물을 에디터 레벨에 생성합니다. */
	ANPBaseRelic* RecreateRelicInEditor();
#endif

protected:
	UFUNCTION()
	void OnRep_SpawnedRelic();

	UFUNCTION()
	void OnRep_IsRelicReleased();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relic Slot")
	TSubclassOf<ANPBaseRelic> RelicClass;

	UPROPERTY(ReplicatedUsing = OnRep_SpawnedRelic, VisibleInstanceOnly, BlueprintReadOnly, Category = "Relic Slot")
	TObjectPtr<ANPBaseRelic> SpawnedRelic;

	UPROPERTY(ReplicatedUsing = OnRep_IsRelicReleased, VisibleInstanceOnly, BlueprintReadOnly, Category = "Relic Slot")
	bool bIsRelicReleased = false;

private:
	void ApplyRelicState();
};
