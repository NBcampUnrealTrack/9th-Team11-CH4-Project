#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "NPPSOPrecacheComponent.generated.h"

DECLARE_DELEGATE_FourParams(FNPPSOPrecacheProgress, float, uint32, double, double);

UCLASS()
class NOPHOTOS_API UNPPSOPrecacheComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	void WaitForCompletion(FSimpleDelegate OnCompleted, FNPPSOPrecacheProgress OnProgress);
	void CancelWait();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void CheckCompletion();

	FTimerHandle CheckTimer;
	FSimpleDelegate Completion;
	FNPPSOPrecacheProgress Progress;
	uint32 PeakRemaining = 0;
	double SpeedSampleStartedAt = 0.0;
	uint32 SpeedSampleRemaining = 0;
	double RecentTasksPerSecond = -1.0;
	double WaitStartedAt = -1.0;
	double LastProgressLogAt = 0.0;
	uint32 LastLoggedRemaining = 0;
};
