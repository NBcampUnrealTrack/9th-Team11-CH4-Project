#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NPRoomGenerationHelper.generated.h"

class UArrowComponent;
class USceneComponent;
class UWorld;

/** 방 후보와 배치 슬롯을 제공하고 생성 Seed를 복제하는 레벨 헬퍼입니다. */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPRoomGenerationHelper : public AActor
{
	GENERATED_BODY()

public:
	ANPRoomGenerationHelper();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room Generation")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room Generation|Slots")
	TObjectPtr<UArrowComponent> TopRightSlot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room Generation|Slots")
	TObjectPtr<UArrowComponent> TopLeftSlot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room Generation|Slots")
	TObjectPtr<UArrowComponent> BottomRightSlot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room Generation|Slots")
	TObjectPtr<UArrowComponent> BottomLeftSlot;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Room Generation")
	TArray<TSoftObjectPtr<UWorld>> Rooms;

private:
	UFUNCTION()
	void OnRep_LayoutSeed();

	void RequestRoomGeneration();

	UPROPERTY(ReplicatedUsing = OnRep_LayoutSeed)
	int32 LayoutSeed = 0;
};
