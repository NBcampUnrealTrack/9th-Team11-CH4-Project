#include "NPRoomRelicCollector.h"

#include "EngineUtils.h"
#include "Gameplay/Relic/NPBaseRelic.h"

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
