#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Gameplay/MapEvents/NPMapEventDefinition.h"
#include "NPSantaGiftTypes.h"
#include "NPSantaEventDefinition.generated.h"

class ANPSantaFlightActor;
class ANPSantaGiftActor;
class ANPBaseRelic;

/** 공통 메타데이터/비행 시간, 선물 투하 설정과 랜덤 유물 후보를 관리합니다. */
UCLASS(BlueprintType)
class NOPHOTOS_API UNPSantaEventDefinition : public UNPMapEventDefinition
{
	GENERATED_BODY()

public:
	UNPSantaEventDefinition();
	TSubclassOf<ANPSantaFlightActor> GetSantaClass() const { return SantaClass; }
	FGameplayTag GetRouteGroup() const { return RouteGroup; }
	TSubclassOf<ANPSantaGiftActor> GetGiftClass() const { return GiftClass; }
	const FNPSantaGiftDropSchedule& GetGiftDrops() const { return GiftDrops; }
	const TArray<TSubclassOf<ANPBaseRelic>>& GetRelicClasses() const { return RelicClasses; }
	float GetGiftDropHeightOffset() const { return GiftDropHeightOffset; }

private:
	/** 사용자가 산타/썰매 외형을 설정한 NPSantaFlightActor 파생 BP입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Santa Event", meta=(AllowPrivateAccess="true"))
	TSubclassOf<ANPSantaFlightActor> SantaClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Santa Event", meta=(AllowPrivateAccess="true"))
	FGameplayTag RouteGroup;

	/** 사용자가 선물상자 외형을 설정한 NPSantaGiftActor 자식 BP입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Santa Event|Gifts", meta=(AllowPrivateAccess="true"))
	TSubclassOf<ANPSantaGiftActor> GiftClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Santa Event|Gifts", meta=(AllowPrivateAccess="true"))
	FNPSantaGiftDropSchedule GiftDrops;

	/** 비행 경로의 XY는 유지하고 산타 바로 아래에서 떨어뜨립니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Santa Event|Gifts", meta=(AllowPrivateAccess="true", ClampMin="0.0", Units="cm"))
	float GiftDropHeightOffset = 100.0f;

	/** 각 상자가 개봉할 때 하나를 균등 추첨합니다. 중복 클래스는 한 후보로 취급합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Santa Event|Gifts", meta=(AllowPrivateAccess="true"))
	TArray<TSubclassOf<ANPBaseRelic>> RelicClasses;
};
