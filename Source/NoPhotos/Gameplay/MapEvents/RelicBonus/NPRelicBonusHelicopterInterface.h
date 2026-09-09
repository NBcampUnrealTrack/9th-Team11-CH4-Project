#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "NPRelicBonusHelicopterInterface.generated.h"

UINTERFACE(BlueprintType)
class NOPHOTOS_API UNPRelicBonusHelicopterInterface : public UInterface
{
	GENERATED_BODY()
};

/** RelicBonus 운반체가 제거되기 전에 블루프린트 퇴장 연출을 시작하도록 알립니다. */
class NOPHOTOS_API INPRelicBonusHelicopterInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Relic Bonus Event|Helicopter")
	void BeginRelicBonusDeparture(float DepartureDuration);

	virtual void BeginRelicBonusDeparture_Implementation(float DepartureDuration) {}
};
