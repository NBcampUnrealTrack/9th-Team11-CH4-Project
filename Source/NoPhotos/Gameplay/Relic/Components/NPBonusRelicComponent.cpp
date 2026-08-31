#include "Gameplay/Relic/Components/NPBonusRelicComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/Map/Room/NPRoomRelicCollector.h"
#include "Gameplay/Relic/NPBaseRelic.h"
#include "Gameplay/Relic/Components/NPPlayerBonusQuestComponent.h"
#include "Net/UnrealNetwork.h"
#include "SubSystem/Room/NPRoomGenerateSubsystem.h"

UNPBonusRelicComponent::UNPBonusRelicComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UNPBonusRelicComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UNPBonusRelicComponent, QuestRelics);
}

void UNPBonusRelicComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!IsOwnerAuthority())
	{
		return;
	}

	UNPRoomGenerateSubsystem* RoomGenerateSubsystem = GetWorld()
		? GetWorld()->GetSubsystem<UNPRoomGenerateSubsystem>()
		: nullptr;
	if (!IsValid(RoomGenerateSubsystem))
	{
		return;
	}

	if (RoomGenerateSubsystem->IsGenerationComplete())
	{
		SelectQuestRelicsFromGeneratedRooms();
		return;
	}

	RoomGenerateSubsystem->OnRoomGenerationCompleted.AddUniqueDynamic(
		this,
		&UNPBonusRelicComponent::HandleRoomGenerationCompleted);
}

TArray<ANPBaseRelic*> UNPBonusRelicComponent::GetQuestRelics() const
{
	TArray<ANPBaseRelic*> Result;
	Result.Reserve(QuestRelics.Num());
	for (ANPBaseRelic* Relic : QuestRelics)
	{
		Result.Add(Relic);
	}
	return Result;
}

bool UNPBonusRelicComponent::IsQuestRelic(const ANPBaseRelic* Relic) const
{
	return IsValid(Relic) && QuestRelics.Contains(Relic);
}

ANPBaseRelic* UNPBonusRelicComponent::SelectQuestRelicFromRoom(const TArray<ANPBaseRelic*>& RoomRelics)
{
	if (!IsOwnerAuthority())
	{
		return nullptr;
	}

	TArray<ANPBaseRelic*> Candidates;
	for (ANPBaseRelic* Relic : RoomRelics)
	{
		if (IsValid(Relic) && !IsQuestRelic(Relic))
		{
			Candidates.Add(Relic);
		}
	}

	if (Candidates.IsEmpty())
	{
		return nullptr;
	}

	ANPBaseRelic* SelectedRelic = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
	return AddQuestRelic(SelectedRelic) ? SelectedRelic : nullptr;
}

bool UNPBonusRelicComponent::AddQuestRelic(ANPBaseRelic* Relic)
{
	if (!IsOwnerAuthority() || !IsValid(Relic) || IsQuestRelic(Relic))
	{
		return false;
	}

	QuestRelics.Add(Relic);
	if (!QuestRelicTag.IsNone())
	{
		Relic->Tags.AddUnique(QuestRelicTag);
	}

	return true;
}

void UNPBonusRelicComponent::ClearQuestRelics()
{
	if (!IsOwnerAuthority())
	{
		return;
	}

	if (!QuestRelicTag.IsNone())
	{
		for (ANPBaseRelic* Relic : QuestRelics)
		{
			if (IsValid(Relic))
			{
				Relic->Tags.Remove(QuestRelicTag);
			}
		}
	}

	QuestRelics.Reset();
}

void UNPBonusRelicComponent::HandleRoomGenerationCompleted()
{
	SelectQuestRelicsFromGeneratedRooms();
}

void UNPBonusRelicComponent::SelectQuestRelicsFromGeneratedRooms()
{
	if (!IsOwnerAuthority())
	{
		return;
	}

	UNPRoomGenerateSubsystem* RoomGenerateSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UNPRoomGenerateSubsystem>()	: nullptr;
	if (!IsValid(RoomGenerateSubsystem) || !RoomGenerateSubsystem->IsGenerationComplete())
	{
		return;
	}

	ClearQuestRelics();
	for (const FNPRoomInstanceInfo& RoomInfo : RoomGenerateSubsystem->GetGeneratedRooms())
	{
		const ANPRoomRelicCollector* RelicCollector = RoomInfo.RelicCollector.Get();
		if (!IsValid(RelicCollector))
		{
			continue;
		}

		TArray<ANPBaseRelic*> RoomRelics;
		for (ANPBaseRelic* Relic : RelicCollector->GetRelics())
		{
			if (IsValid(Relic))
			{
				RoomRelics.Add(Relic);
			}
		}

		SelectQuestRelicFromRoom(RoomRelics);
	}

	DistributeQuestRelicsToPlayers();
}

bool UNPBonusRelicComponent::DistributeQuestRelicsToPlayers()
{
	if (!IsOwnerAuthority()	|| QuestRelicsPerPlayer <= 0 || QuestRelics.Num() < QuestRelicsPerPlayer || !GetWorld())
	{
		return false;
	}

	TArray<APlayerController*> PlayerControllers;
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PlayerController = Iterator->Get();
		if (IsValid(PlayerController) && IsValid(PlayerController->FindComponentByClass<UNPPlayerBonusQuestComponent>()))
		{
			PlayerControllers.Add(PlayerController);
		}
	}

	if (PlayerControllers.IsEmpty())
	{
		return false;
	}

	for (int32 Index = PlayerControllers.Num() - 1; Index > 0; --Index)
	{
		PlayerControllers.Swap(Index, FMath::RandRange(0, Index));
	}

	TArray<int32> QuestRelicIndices;
	QuestRelicIndices.Reserve(QuestRelics.Num());
	for (int32 Index = 0; Index < QuestRelics.Num(); ++Index)
	{
		QuestRelicIndices.Add(Index);
	}
	for (int32 Index = QuestRelicIndices.Num() - 1; Index > 0; --Index)
	{
		QuestRelicIndices.Swap(Index, FMath::RandRange(0, Index));
	}

	bool bAssignedAnyPlayer = false;
	for (int32 PlayerIndex = 0; PlayerIndex < PlayerControllers.Num(); ++PlayerIndex)
	{
		UNPPlayerBonusQuestComponent* PlayerQuestComponent = PlayerControllers[PlayerIndex]->FindComponentByClass<UNPPlayerBonusQuestComponent>();
		if (!IsValid(PlayerQuestComponent))
		{
			continue;
		}

		TArray<ANPBaseRelic*> AssignedRelics;
		AssignedRelics.Reserve(QuestRelicsPerPlayer);
		const int32 StartIndex = PlayerIndex % QuestRelicIndices.Num();
		for (int32 Offset = 0; Offset < QuestRelicsPerPlayer; ++Offset)
		{
			const int32 RelicIndex = QuestRelicIndices[(StartIndex + Offset) % QuestRelicIndices.Num()];
			AssignedRelics.Add(QuestRelics[RelicIndex]);
		}

		bAssignedAnyPlayer |= PlayerQuestComponent->SetAssignedQuestRelics(AssignedRelics);
	}

	return bAssignedAnyPlayer;
}

bool UNPBonusRelicComponent::IsOwnerAuthority() const
{
	const AActor* Owner = GetOwner();
	return IsValid(Owner) && Owner->HasAuthority();
}
