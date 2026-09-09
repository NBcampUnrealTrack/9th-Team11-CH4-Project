#pragma once

#include "CoreMinimal.h"
#include "Core/Asset/NPAssetLoadTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "NPRoomGenerateSubsystem.generated.h"

class ANPRoomRelicCollector;
class ULevelStreamingDynamic;
class UWorld;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNPOnRoomGenerationCompleted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNPOnRoomGenerationFailed);

USTRUCT()
struct NOPHOTOS_API FNPRoomInstanceInfo
{
	GENERATED_BODY()

	int32 SlotIndex = INDEX_NONE;
	TSoftObjectPtr<UWorld> RoomLevel;
	FTransform RoomTransform = FTransform::Identity;

	UPROPERTY(Transient)
	TObjectPtr<ULevelStreamingDynamic> StreamingLevel;

	UPROPERTY(Transient)
	TWeakObjectPtr<ANPRoomRelicCollector> RelicCollector;
};

/** 방 레벨 인스턴스의 선택, 생성 및 완료 상태를 관리합니다. */
UCLASS()
class NOPHOTOS_API UNPRoomGenerateSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	bool GenerateRooms(
		const TArray<TSoftObjectPtr<UWorld>>& Rooms,
		const TArray<FTransform>& SlotTransforms,
		int32 LayoutSeed);

	UFUNCTION(BlueprintPure, Category="Room Generation")
	bool IsGenerationComplete() const { return bGenerationComplete; }

	UFUNCTION(BlueprintPure, Category="Room Generation")
	bool HasGenerationFailed() const { return bGenerationFailed; }

	/** Current room-generation progress in the normalized 0.0-1.0 range. */
	UFUNCTION(BlueprintPure, Category="Room Generation")
	float GetGenerationProgress() const;

	const TArray<FNPRoomInstanceInfo>& GetGeneratedRooms() const
	{
		return GeneratedRooms;
	}

	UPROPERTY(BlueprintAssignable, Category="Room Generation")
	FNPOnRoomGenerationCompleted OnRoomGenerationCompleted;

	UPROPERTY(BlueprintAssignable, Category="Room Generation")
	FNPOnRoomGenerationFailed OnRoomGenerationFailed;

private:
	void HandleRoomAssetsLoaded(const FNPAssetLoadResult& Result);
	void CreateSelectedRoomInstances();
	void FailGeneration();

	UFUNCTION()
	void HandleLevelShown();

	bool CollectRoomRelicCollectors();

	UPROPERTY(Transient)
	TArray<FNPRoomInstanceInfo> GeneratedRooms;

	TArray<TSoftObjectPtr<UWorld>> SelectedRooms;
	TArray<FTransform> SelectedRoomTransforms;
	FNPAssetLoadRequestId AssetLoadRequestId;

	int32 ExpectedRoomCount = 0;
	bool bGenerationStarted = false;
	bool bGenerationComplete = false;
	bool bGenerationFailed = false;
};
