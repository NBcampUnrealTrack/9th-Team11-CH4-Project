#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NPLegacyMapEventManager.generated.h"

class ANPMapEvent;

/**
 * Initial map-event manager structure kept for portfolio comparison.
 *
 * This actor intentionally demonstrates the former design in which the
 * manager directly owns concrete event classes and all scheduling duties.
 */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPLegacyMapEventManager : public AActor
{
	GENERATED_BODY()

public:
	ANPLegacyMapEventManager();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Legacy Map Event")
	void StartEventScheduling();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Legacy Map Event")
	void StopEventScheduling();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Legacy Map Event")
	bool TriggerRandomEvent();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Legacy Map Event")
	TArray<TSubclassOf<ANPMapEvent>> EventClasses;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Legacy Map Event")
	bool bStartAutomatically = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Legacy Map Event", meta = (ClampMin = "0.0"))
	float InitialDelay = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Legacy Map Event", meta = (ClampMin = "0.1"))
	float MinimumInterval = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Legacy Map Event", meta = (ClampMin = "0.1"))
	float MaximumInterval = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Legacy Map Event")
	bool bAllowConcurrentEvents = false;

private:
	void CreateEventInstances();
	void ScheduleNextEvent(float Delay);
	void HandleEventTimer();
	bool HasActiveEvent() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ANPMapEvent>> EventInstances;

	FTimerHandle EventTimer;
};
