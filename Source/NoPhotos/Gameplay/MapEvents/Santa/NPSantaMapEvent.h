#pragma once

#include "CoreMinimal.h"
#include "Gameplay/MapEvents/NPMapEvent.h"
#include "NPSantaGiftTypes.h"
#include "NPSantaMapEvent.generated.h"

class ANPSantaFlightActor;
class ANPSantaFlightRoute;
class ANPSantaGiftActor;
class ANPBaseRelic;
class UNPSantaEventDefinition;

/** 산타 비행과 서버 선물 투하를 담당합니다. 착지/개봉/유물 생성은 독립된 선물 액터가 처리합니다. */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPSantaMapEvent : public ANPMapEvent
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 비행 사이 대기 중, 초기 복제 중 또는 이벤트 종료 후에는 null일 수 있습니다. */
	UFUNCTION(BlueprintPure, Category="Santa Event")
	ANPSantaFlightActor* GetSanta() const { return SpawnedSanta; }

protected:
	virtual void ApplyEventState_Implementation(bool bNewActive) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool StartSantaFlight();
	void FinishSantaFlight();
	void ScheduleNextFlight();
	void StartNextFlight();
	void StartGiftDrops(const UNPSantaEventDefinition* Definition);
	void ScheduleNextGiftDrop();
	void DropGift();
	void CleanupFlight();
	void CleanupEvent();
	void ScheduleFailedFinish();
	void FinishFailedStart();

	UFUNCTION()
	void OnSantaDestroyed(AActor* DestroyedActor);

	UPROPERTY(Replicated, Transient)
	TObjectPtr<ANPSantaFlightActor> SpawnedSanta;

	TWeakObjectPtr<ANPSantaFlightRoute> LastFlightRoute;
	FTimerHandle FlightEndTimer;
	FTimerHandle RespawnTimer;
	FTimerHandle FailedStartTimer;
	FTimerHandle GiftDropTimer;
	FNPSantaGiftDropSchedule ActiveGiftDrops;
	int32 NextGiftIndex = 0;
	float ActiveDropHeightOffset = 100.0f;

	UPROPERTY(Transient)
	TSubclassOf<ANPSantaGiftActor> ActiveGiftClass;
	UPROPERTY(Transient)
	TArray<TSubclassOf<ANPBaseRelic>> ActiveRelicClasses;
	UPROPERTY(Transient)
	TSubclassOf<ANPBaseRelic> ActivePrimaryRelicClass;
	float ActivePrimaryRelicChancePercent = 0.0f;
};
