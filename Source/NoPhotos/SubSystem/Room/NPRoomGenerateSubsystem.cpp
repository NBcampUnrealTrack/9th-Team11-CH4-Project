#include "NPRoomGenerateSubsystem.h"

#include "Core/Asset/NPAssetLoadSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "Gameplay/Map/Room/NPRoomRelicCollector.h"
#include "Gameplay/Relic/NPBaseRelic.h"

DEFINE_LOG_CATEGORY_STATIC(LogNPRoomGenerate, Log, All);

void UNPRoomGenerateSubsystem::Deinitialize()
{
	if (AssetLoadRequestId.IsValid())
	{
		if (UGameInstance* GameInstance = GetWorld()
			? GetWorld()->GetGameInstance()
			: nullptr)
		{
			if (UNPAssetLoadSubsystem* AssetLoader =
				GameInstance->GetSubsystem<UNPAssetLoadSubsystem>())
			{
				AssetLoader->CancelSoftPathRequest(AssetLoadRequestId);
			}
		}
	}
	for (const FNPRoomInstanceInfo& RoomInfo : GeneratedRooms)
	{
		if (ULevelStreamingDynamic* StreamingLevel = RoomInfo.StreamingLevel;
			IsValid(StreamingLevel))
		{
			StreamingLevel->OnLevelShown.RemoveDynamic(
				this,
				&UNPRoomGenerateSubsystem::HandleLevelShown);
		}
	}

	GeneratedRooms.Reset();
	SelectedRooms.Reset();
	SelectedRoomTransforms.Reset();
	AssetLoadRequestId = {};
	ExpectedRoomCount = 0;
	bGenerationStarted = false;
	bGenerationComplete = false;
	bGenerationFailed = false;
	Super::Deinitialize();
}

bool UNPRoomGenerateSubsystem::GenerateRooms(
	const TArray<TSoftObjectPtr<UWorld>>& Rooms,
	const TArray<FTransform>& SlotTransforms,
	const int32 LayoutSeed)
{
	if (bGenerationStarted)
	{
		return false;
	}

	bGenerationStarted = true;
	if (LayoutSeed == 0 || Rooms.IsEmpty() || SlotTransforms.IsEmpty())
	{
		FailGeneration();
		return false;
	}

	TArray<int32> RoomOrder;
	RoomOrder.Reserve(Rooms.Num());
	for (int32 RoomIndex = 0; RoomIndex < Rooms.Num(); ++RoomIndex)
	{
		RoomOrder.Add(RoomIndex);
	}

	FRandomStream RandomStream(LayoutSeed);
	for (int32 Index = RoomOrder.Num() - 1; Index > 0; --Index)
	{
		RoomOrder.Swap(Index, RandomStream.RandRange(0, Index));
	}

	const int32 MaximumRoomCount = FMath::Min(RoomOrder.Num(), SlotTransforms.Num());
	TArray<FSoftObjectPath> RoomPaths;
	for (int32 SlotIndex = 0; SlotIndex < MaximumRoomCount; ++SlotIndex)
	{
		const TSoftObjectPtr<UWorld>& Room = Rooms[RoomOrder[SlotIndex]];
		if (Room.IsNull())
		{
			continue;
		}

		SelectedRooms.Add(Room);
		SelectedRoomTransforms.Add(SlotTransforms[SlotIndex]);
		RoomPaths.AddUnique(Room.ToSoftObjectPath());
	}

	ExpectedRoomCount = SelectedRooms.Num();
	UE_LOG(LogNPRoomGenerate, Log,
		TEXT("Preloading selected room assets. Rooms=%d UniquePaths=%d"),
		ExpectedRoomCount,
		RoomPaths.Num());
	UGameInstance* GameInstance = GetWorld()
		? GetWorld()->GetGameInstance()
		: nullptr;
	UNPAssetLoadSubsystem* AssetLoader = GameInstance
		? GameInstance->GetSubsystem<UNPAssetLoadSubsystem>()
		: nullptr;
	if (!AssetLoader || ExpectedRoomCount == 0)
	{
		FailGeneration();
		return false;
	}

	AssetLoadRequestId = AssetLoader->LoadSoftPathsAsync(
		this,
		RoomPaths,
		FNPOnAssetLoadComplete::CreateUObject(
			this,
			&UNPRoomGenerateSubsystem::HandleRoomAssetsLoaded));
	return AssetLoadRequestId.IsValid();
}

void UNPRoomGenerateSubsystem::HandleRoomAssetsLoaded(
	const FNPAssetLoadResult& Result)
{
	AssetLoadRequestId = {};
	if (!Result.IsSuccess())
	{
		UE_LOG(LogNPRoomGenerate, Error,
			TEXT("Room asset preload failed. Failure=%d"),
			static_cast<int32>(Result.Failure));
		FailGeneration();
		return;
	}

	UE_LOG(LogNPRoomGenerate, Log,
		TEXT("Selected room assets preloaded. Objects=%d"),
		Result.LoadedObjects.Num());
	CreateSelectedRoomInstances();
}

void UNPRoomGenerateSubsystem::CreateSelectedRoomInstances()
{
	for (int32 RoomIndex = 0; RoomIndex < SelectedRooms.Num(); ++RoomIndex)
	{
		const TSoftObjectPtr<UWorld>& Room = SelectedRooms[RoomIndex];

		bool bLoadSucceeded = false;
		const FString InstanceName = FString::Printf(
			TEXT("NP_RoomSlot_%d"),
			RoomIndex);
		ULevelStreamingDynamic* LoadedRoom =
			ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(
				GetWorld(),
				Room,
				SelectedRoomTransforms[RoomIndex],
				bLoadSucceeded,
				InstanceName);
		if (!bLoadSucceeded || !IsValid(LoadedRoom))
		{
			UE_LOG(LogNPRoomGenerate, Error,
				TEXT("Room level instance creation failed. Room=%s"),
				*Room.ToSoftObjectPath().ToString());
			FailGeneration();
			return;
		}

		LoadedRoom->OnLevelShown.AddDynamic(
			this,
			&UNPRoomGenerateSubsystem::HandleLevelShown);

		FNPRoomInstanceInfo& RoomInfo = GeneratedRooms.AddDefaulted_GetRef();
		RoomInfo.SlotIndex = RoomIndex;
		RoomInfo.RoomLevel = Room;
		RoomInfo.RoomTransform = SelectedRoomTransforms[RoomIndex];
		RoomInfo.StreamingLevel = LoadedRoom;
		UE_LOG(LogNPRoomGenerate, Log,
			TEXT("Room level instance requested. Slot=%d Room=%s"),
			RoomIndex,
			*Room.ToSoftObjectPath().ToString());
	}

	HandleLevelShown();
}

void UNPRoomGenerateSubsystem::FailGeneration()
{
	if (bGenerationFailed)
	{
		return;
	}

	bGenerationComplete = false;
	bGenerationFailed = true;
	OnRoomGenerationFailed.Broadcast();
}

void UNPRoomGenerateSubsystem::HandleLevelShown()
{
	if (bGenerationComplete || ExpectedRoomCount == 0 ||
		GeneratedRooms.Num() != ExpectedRoomCount)
	{
		return;
	}

	for (const FNPRoomInstanceInfo& RoomInfo : GeneratedRooms)
	{
		if (!IsValid(RoomInfo.StreamingLevel) ||
			!RoomInfo.StreamingLevel->IsLevelVisible())
		{
			return;
		}
	}

	// Level visibility is the world-readiness boundary. A room may intentionally
	// omit a relic collector, so collector discovery must not deadlock loading.
	CollectRoomRelicCollectors();

	bGenerationComplete = true;
	bGenerationFailed = false;
	SelectedRooms.Reset();
	SelectedRoomTransforms.Reset();
	UE_LOG(LogNPRoomGenerate, Log,
		TEXT("Room generation completed. VisibleRooms=%d"),
		GeneratedRooms.Num());
	OnRoomGenerationCompleted.Broadcast();
}

bool UNPRoomGenerateSubsystem::CollectRoomRelicCollectors()
{
	bool bCollectedAllRooms = true;
	for (FNPRoomInstanceInfo& RoomInfo : GeneratedRooms)
	{
		RoomInfo.RelicCollector.Reset();

		ULevel* LoadedLevel = IsValid(RoomInfo.StreamingLevel)
			? RoomInfo.StreamingLevel->GetLoadedLevel()
			: nullptr;
		if (!LoadedLevel)
		{
			bCollectedAllRooms = false;
			continue;
		}

		for (AActor* Actor : LoadedLevel->Actors)
		{
			if (ANPRoomRelicCollector* RelicCollector =
				Cast<ANPRoomRelicCollector>(Actor))
			{
				RoomInfo.RelicCollector = RelicCollector;
				break;
			}
		}

		if (!RoomInfo.RelicCollector.IsValid())
		{
			UE_LOG(LogNPRoomGenerate, Warning,
				TEXT("Room has no relic collector; world loading will continue. Slot=%d Room=%s"),
				RoomInfo.SlotIndex,
				*RoomInfo.RoomLevel.ToSoftObjectPath().ToString());
		}
	}

	return bCollectedAllRooms;
}
