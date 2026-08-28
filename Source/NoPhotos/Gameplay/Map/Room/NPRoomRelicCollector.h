#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "NPRoomRelicCollector.generated.h"

class ANPBaseRelic;

/** 같은 방 레벨에 배치된 유물 참조를 저장하는 레벨 헬퍼입니다. */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPRoomRelicCollector : public AInfo
{
	GENERATED_BODY()

public:
	ANPRoomRelicCollector();

	/** 이 Collector와 같은 레벨에 배치된 유물을 다시 수집합니다. */
	UFUNCTION(CallInEditor, Category="Room|Relic", meta=(DisplayName="레벨 유물 다시 수집"))
	void CollectRelics();

	/** 현재 방에서 퀘스트 대상으로 사용할 유물을 반환합니다. */
	UFUNCTION(BlueprintPure, Category="Room|Relic")
	ANPBaseRelic* GetQuestRelic() const;

	const TArray<TObjectPtr<ANPBaseRelic>>& GetRelics() const { return Relics; }

protected:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Room|Relic")
	TArray<TObjectPtr<ANPBaseRelic>> Relics;
};
