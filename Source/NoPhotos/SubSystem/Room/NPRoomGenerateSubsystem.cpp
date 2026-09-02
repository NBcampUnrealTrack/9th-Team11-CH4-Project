#include "NPRoomGenerateSubsystem.h"

#include "Engine/Level.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "Gameplay/Map/Room/NPRoomRelicCollector.h"
#include "Gameplay/Relic/NPBaseRelic.h"

DEFINE_LOG_CATEGORY_STATIC(LogNPRoomGenerate, Log, All);

void UNPRoomGenerateSubsystem::Deinitialize()
{
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
	ExpectedRoomCount = 0;
	bGenerationStarted = false;
	bGenerationComplete = false;
	Super::Deinitialize();
}

bool UNPRoomGenerateSubsystem::GenerateRooms(
	const TArray<TSoftObjectPtr<UWorld>>& Rooms,
	const TArray<FTransform>& SlotTransforms,
	const int32 LayoutSeed)
{
	if (bGenerationStarted || LayoutSeed == 0 || Rooms.IsEmpty() ||
		SlotTransforms.IsEmpty())
	{
		return false;
	}

	bGenerationStarted = true;

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

	ExpectedRoomCount = FMath::Min(RoomOrder.Num(), SlotTransforms.Num());
	for (int32 SlotIndex = 0; SlotIndex < ExpectedRoomCount; ++SlotIndex)
	{
		const TSoftObjectPtr<UWorld>& Room = Rooms[RoomOrder[SlotIndex]];
		if (Room.IsNull())
		{
			continue;
		}

		bool bLoadSucceeded = false;
		//TODO 레벨인스턴스 불러오는 기능을 편의 기능 클래스로 레핑하면 좋을듯 함.
		const FString InstanceName = FString::Printf(
			TEXT("NP_RoomSlot_%d"),
			SlotIndex);
		ULevelStreamingDynamic* LoadedRoom =
			ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(
				GetWorld(),
				Room,
				SlotTransforms[SlotIndex],
				bLoadSucceeded,
				InstanceName);
		if (!bLoadSucceeded || !IsValid(LoadedRoom))
		{
			continue;
		}

		LoadedRoom->OnLevelShown.AddDynamic(
			this,
			&UNPRoomGenerateSubsystem::HandleLevelShown);

		FNPRoomInstanceInfo& RoomInfo = GeneratedRooms.AddDefaulted_GetRef();
		RoomInfo.SlotIndex = SlotIndex;
		RoomInfo.RoomLevel = Room;
		RoomInfo.RoomTransform = SlotTransforms[SlotIndex];
		RoomInfo.StreamingLevel = LoadedRoom;
	}

	HandleLevelShown();
	return GeneratedRooms.Num() == ExpectedRoomCount;
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

	if (!CollectRoomRelicCollectors())
	{
		return;
	}

	bGenerationComplete = true;
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
			bCollectedAllRooms = false;
		}
	}

	return bCollectedAllRooms;
}