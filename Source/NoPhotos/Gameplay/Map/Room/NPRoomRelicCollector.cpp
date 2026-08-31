#include "NPRoomRelicCollector.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Gameplay/Relic/NPBaseRelic.h"
#include "Gameplay/Relic/Components/NPRelicSlotComponent.h"

ANPRoomRelicCollector::ANPRoomRelicCollector()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
}

ANPBaseRelic* ANPRoomRelicCollector::GetQuestRelic() const
{
	ANPBaseRelic* QuestRelic = Relics.IsEmpty() ? nullptr : Relics[0].Get();
	return IsValid(QuestRelic) ? QuestRelic : nullptr;
}

void ANPRoomRelicCollector::CreateSlotRelicsAndCollect()
{
#if WITH_EDITOR
	UWorld* World = GetWorld();
	if (!World || World->IsGameWorld())
	{
		return;
	}

	TArray<UNPRelicSlotComponent*> RelicSlots;
	for (TActorIterator<AActor> Iterator(World); Iterator; ++Iterator)
	{
		AActor* Actor = *Iterator;
		if (!IsValid(Actor) || Actor->GetLevel() != GetLevel())
		{
			continue;
		}

		TArray<UNPRelicSlotComponent*> ActorRelicSlots;
		Actor->GetComponents(ActorRelicSlots);
		RelicSlots.Append(ActorRelicSlots);
	}

	for (UNPRelicSlotComponent* RelicSlot : RelicSlots)
	{
		if (IsValid(RelicSlot))
		{
			RelicSlot->CreateRelicInEditor();
		}
	}

	CollectRelics();
#endif
}

void ANPRoomRelicCollector::CollectRelics()
{
#if WITH_EDITOR
	Modify();
#endif

	Relics.Reset();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<ANPBaseRelic> Iterator(World); Iterator; ++Iterator)
	{
		ANPBaseRelic* Relic = *Iterator;
		if (IsValid(Relic) && Relic->GetLevel() == GetLevel())
		{
			Relics.Add(Relic);
		}
	}

#if WITH_EDITOR
	MarkPackageDirty();
#endif
}
