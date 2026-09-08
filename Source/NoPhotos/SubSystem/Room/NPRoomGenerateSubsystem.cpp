#include "NPRoomGenerateSubsystem.h"

#include "Core/Asset/NPAssetLoadSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "Gameplay/Map/Room/NPRoomRelicCollector.h"
#include "Gameplay/Relic/NPBaseRelic.h"

DEFINE_LOG_CATEGORY_STATIC(LogNPRoomGenerate, Log, All);

namespace
{
	const TCHAR* GetAssetLoadFailureName(const ENPAssetLoadFailure Failure)
	{
		switch (Failure)
		{
		case ENPAssetLoadFailure::None: return TEXT("None");
		case ENPAssetLoadFailure::InvalidRequest: return TEXT("InvalidRequest");
		case ENPAssetLoadFailure::OwnerInvalid: return TEXT("OwnerInvalid");
		case ENPAssetLoadFailure::WorldChanged: return TEXT("WorldChanged");
		case ENPAssetLoadFailure::Canceled: return TEXT("Canceled");
		case ENPAssetLoadFailure::LoadFailed: return TEXT("LoadFailed");
		default: return TEXT("Unknown");
		}
	}
}

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
		UE_LOG(LogNPRoomGenerate, Warning,
			TEXT("GenerateRooms rejected because generation already started. World=%s Complete=%s Failed=%s"),
			*GetNameSafe(GetWorld()),
			bGenerationComplete ? TEXT("true") : TEXT("false"),
			bGenerationFailed ? TEXT("true") : TEXT("false"));
		return false;
	}

	bGenerationStarted = true;
	if (LayoutSeed == 0 || Rooms.IsEmpty() || SlotTransforms.IsEmpty())
	{
		UE_LOG(LogNPRoomGenerate, Error,
			TEXT("GenerateRooms received invalid input. World=%s LayoutSeed=%d Rooms=%d SlotTransforms=%d"),
			*GetNameSafe(GetWorld()), LayoutSeed, Rooms.Num(), SlotTransforms.Num());
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
			UE_LOG(LogNPRoomGenerate, Warning,
				TEXT("Skipping null room reference. SourceIndex=%d SlotIndex=%d"),
				RoomOrder[SlotIndex], SlotIndex);
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
		UE_LOG(LogNPRoomGenerate, Error,
			TEXT("Room preload cannot start. World=%s GameInstance=%s AssetLoader=%s SelectedRooms=%d"),
			*GetNameSafe(GetWorld()), *GetNameSafe(GameInstance),
			*GetNameSafe(AssetLoader), ExpectedRoomCount);
		FailGeneration();
		return false;
	}

	AssetLoadRequestId = AssetLoader->LoadSoftPathsAsync(
		this,
		RoomPaths,
		FNPOnAssetLoadComplete::CreateUObject(
			this,
			&UNPRoomGenerateSubsystem::HandleRoomAssetsLoaded));
	if (!AssetLoadRequestId.IsValid())
	{
		UE_LOG(LogNPRoomGenerate, Error,
			TEXT("Room preload request returned an invalid RequestId. World=%s Paths=%d"),
			*GetNameSafe(GetWorld()), RoomPaths.Num());
	}
	return AssetLoadRequestId.IsValid();
}

void UNPRoomGenerateSubsystem::HandleRoomAssetsLoaded(
	const FNPAssetLoadResult& Result)
{
	AssetLoadRequestId = {};
	if (!Result.IsSuccess())
	{
		UE_LOG(LogNPRoomGenerate, Error,
			TEXT("Room asset preload failed. RequestId=%s Status=%d Failure=%s(%d) LoadedObjects=%d ExpectedRooms=%d"),
			*Result.RequestId.Value.ToString(), static_cast<int32>(Result.Status),
			GetAssetLoadFailureName(Result.Failure), static_cast<int32>(Result.Failure),
			Result.LoadedObjects.Num(), ExpectedRoomCount);
		for (const TSoftObjectPtr<UWorld>& SelectedRoom : SelectedRooms)
		{
			UE_LOG(LogNPRoomGenerate, Error,
				TEXT("Room preload failure candidate. Path=%s Resolved=%s"),
				*SelectedRoom.ToSoftObjectPath().ToString(),
				SelectedRoom.Get() ? TEXT("true") : TEXT("false"));
		}
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
				TEXT("Room level instance creation failed. Slot=%d Room=%s Instance=%s LoadSucceeded=%s StreamingLevel=%s Transform=%s World=%s"),
				RoomIndex, *Room.ToSoftObjectPath().ToString(), *InstanceName,
				bLoadSucceeded ? TEXT("true") : TEXT("false"),
				*GetNameSafe(LoadedRoom),
				*SelectedRoomTransforms[RoomIndex].ToHumanReadableString(),
				*GetNameSafe(GetWorld()));
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
		UE_LOG(LogNPRoomGenerate, Verbose,
			TEXT("Duplicate room generation failure ignored. World=%s"),
			*GetNameSafe(GetWorld()));
		return;
	}

	UE_LOG(LogNPRoomGenerate, Error,
		TEXT("Room generation marked failed. World=%s ExpectedRooms=%d GeneratedRooms=%d SelectedRooms=%d RequestActive=%s"),
		*GetNameSafe(GetWorld()), ExpectedRoomCount, GeneratedRooms.Num(),
		SelectedRooms.Num(), AssetLoadRequestId.IsValid() ? TEXT("true") : TEXT("false"));
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
			UE_LOG(LogNPRoomGenerate, Log,
				TEXT("Waiting for room visibility. Slot=%d Room=%s StreamingLevel=%s LoadedLevel=%s Visible=%s"),
				RoomInfo.SlotIndex,
				*RoomInfo.RoomLevel.ToSoftObjectPath().ToString(),
				*GetNameSafe(RoomInfo.StreamingLevel),
				*GetNameSafe(IsValid(RoomInfo.StreamingLevel)
					? RoomInfo.StreamingLevel->GetLoadedLevel() : nullptr),
				IsValid(RoomInfo.StreamingLevel) && RoomInfo.StreamingLevel->IsLevelVisible()
					? TEXT("true") : TEXT("false"));
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
