#include "Gameplay/Relic/Components/NPBonusRelicComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/Map/Room/NPRoomRelicCollector.h"
#include "Gameplay/Relic/NPBaseRelic.h"
#include "Gameplay/Relic/Components/NPPlayerBonusQuestComponent.h"
#include "Net/UnrealNetwork.h"
#include "SubSystem/Room/NPRoomGenerateSubsystem.h"

namespace
{
void BuildQuestRelicCombinations(
	const int32 RelicCount,
	const int32 RelicsPerPlayer,
	const int32 StartIndex,
	TArray<int32>& CurrentCombination,
	TArray<TArray<int32>>& OutCombinations)
{
	if (CurrentCombination.Num() == RelicsPerPlayer)
	{
		OutCombinations.Add(CurrentCombination);
		return;
	}

	const int32 RemainingCount = RelicsPerPlayer - CurrentCombination.Num();
	for (int32 Index = StartIndex; Index <= RelicCount - RemainingCount; ++Index)
	{
		CurrentCombination.Add(Index);
		BuildQuestRelicCombinations(
			RelicCount,
			RelicsPerPlayer,
			Index + 1,
			CurrentCombination,
			OutCombinations);
		CurrentCombination.Pop();
	}
}
}

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

	TArray<TArray<int32>> Combinations;
	TArray<int32> CurrentCombination;
	BuildQuestRelicCombinations(
		QuestRelics.Num(),
		QuestRelicsPerPlayer,
		0,
		CurrentCombination,
		Combinations);
	if (Combinations.IsEmpty())
	{
		return false;
	}

	TArray<int32> RelicAssignmentCounts;
	RelicAssignmentCounts.Init(0, QuestRelics.Num());
	TArray<int32> CombinationUseCounts;
	CombinationUseCounts.Init(0, Combinations.Num());
	TArray<TArray<int32>> PreviousAssignments;

	bool bAssignedAnyPlayer = false;
	for (int32 PlayerIndex = 0; PlayerIndex < PlayerControllers.Num(); ++PlayerIndex)
	{
		UNPPlayerBonusQuestComponent* PlayerQuestComponent = PlayerControllers[PlayerIndex]->FindComponentByClass<UNPPlayerBonusQuestComponent>();
		if (!IsValid(PlayerQuestComponent))
		{
			continue;
		}

		int32 BestCombinationIndex = INDEX_NONE;
		int32 BestUseCount = MAX_int32;
		int32 BestAssignmentSpread = MAX_int32;
		int32 BestOverlap = MAX_int32;

		for (int32 CombinationIndex = 0; CombinationIndex < Combinations.Num(); ++CombinationIndex)
		{
			const TArray<int32>& Candidate = Combinations[CombinationIndex];
			int32 ProjectedMinCount = MAX_int32;
			int32 ProjectedMaxCount = MIN_int32;
			for (int32 RelicIndex = 0; RelicIndex < QuestRelics.Num(); ++RelicIndex)
			{
				const int32 ProjectedCount = RelicAssignmentCounts[RelicIndex]
					+ (Candidate.Contains(RelicIndex) ? 1 : 0);
				ProjectedMinCount = FMath::Min(ProjectedMinCount, ProjectedCount);
				ProjectedMaxCount = FMath::Max(ProjectedMaxCount, ProjectedCount);
			}

			int32 TotalOverlap = 0;
			for (const TArray<int32>& PreviousAssignment : PreviousAssignments)
			{
				for (const int32 RelicIndex : Candidate)
				{
					TotalOverlap += PreviousAssignment.Contains(RelicIndex) ? 1 : 0;
				}
			}

			const int32 CandidateUseCount = CombinationUseCounts[CombinationIndex];
			const int32 AssignmentSpread = ProjectedMaxCount - ProjectedMinCount;
			const bool bIsBetterCandidate =
				CandidateUseCount < BestUseCount
				|| (CandidateUseCount == BestUseCount
					&& AssignmentSpread < BestAssignmentSpread)
				|| (CandidateUseCount == BestUseCount
					&& AssignmentSpread == BestAssignmentSpread
					&& TotalOverlap < BestOverlap)
				|| (CandidateUseCount == BestUseCount
					&& AssignmentSpread == BestAssignmentSpread
					&& TotalOverlap == BestOverlap
					&& FMath::RandBool());
			if (bIsBetterCandidate)
			{
				BestCombinationIndex = CombinationIndex;
				BestUseCount = CandidateUseCount;
				BestAssignmentSpread = AssignmentSpread;
				BestOverlap = TotalOverlap;
			}
		}

		if (BestCombinationIndex == INDEX_NONE)
		{
			continue;
		}

		const TArray<int32>& SelectedCombination = Combinations[BestCombinationIndex];
		TArray<ANPBaseRelic*> AssignedRelics;
		AssignedRelics.Reserve(SelectedCombination.Num());
		for (const int32 RelicIndex : SelectedCombination)
		{
			AssignedRelics.Add(QuestRelics[RelicIndex]);
		}

		if (PlayerQuestComponent->SetAssignedQuestRelics(AssignedRelics))
		{
			++CombinationUseCounts[BestCombinationIndex];
			PreviousAssignments.Add(SelectedCombination);
			for (const int32 RelicIndex : SelectedCombination)
			{
				++RelicAssignmentCounts[RelicIndex];
			}
			bAssignedAnyPlayer = true;
		}
	}

	return bAssignedAnyPlayer;
}

bool UNPBonusRelicComponent::IsOwnerAuthority() const
{
	const AActor* Owner = GetOwner();
	return IsValid(Owner) && Owner->HasAuthority();
}
