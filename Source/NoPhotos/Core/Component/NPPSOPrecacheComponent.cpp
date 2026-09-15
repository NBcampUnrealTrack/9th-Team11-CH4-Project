#include "Core/Component/NPPSOPrecacheComponent.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "NoPhotos.h"
#include "ShaderPipelineCache.h"

void UNPPSOPrecacheComponent::WaitForCompletion(
	FSimpleDelegate OnCompleted, FNPPSOPrecacheProgress OnProgress)
{
	CancelWait();
	const APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	UWorld* World = GetWorld();
	if (!PlayerController || !PlayerController->IsLocalController() || !World)
	{
		return;
	}

	Completion = MoveTemp(OnCompleted);
	Progress = MoveTemp(OnProgress);
	WaitStartedAt = FPlatformTime::Seconds();
	LastProgressLogAt = WaitStartedAt;
	LastLoggedRemaining = FShaderPipelineCache::NumPrecompilesRemaining();
	PeakRemaining = LastLoggedRemaining;
	SpeedSampleStartedAt = WaitStartedAt;
	SpeedSampleRemaining = LastLoggedRemaining;
	RecentTasksPerSecond = -1.0;
	UE_LOG(LogNoPhotos, Log, TEXT("[PSO] Wait started. Owner=%s Remaining=%u"),
		*GetNameSafe(GetOwner()), LastLoggedRemaining);
	World->GetTimerManager().SetTimer(
		CheckTimer, this, &ThisClass::CheckCompletion, 0.1f, true);
	Progress.ExecuteIfBound(PeakRemaining == 0 ? 1.0f : 0.0f, LastLoggedRemaining, 0.0, RecentTasksPerSecond);
}

void UNPPSOPrecacheComponent::CancelWait()
{
	if (WaitStartedAt >= 0.0)
	{
		UE_LOG(LogNoPhotos, Log, TEXT("[PSO] Wait cancelled. Owner=%s Remaining=%u Elapsed=%.2fs"),
			*GetNameSafe(GetOwner()), FShaderPipelineCache::NumPrecompilesRemaining(),
			FPlatformTime::Seconds() - WaitStartedAt);
		WaitStartedAt = -1.0;
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CheckTimer);
	}
	Completion.Unbind();
	Progress.Unbind();
}

void UNPPSOPrecacheComponent::CheckCompletion()
{
	const uint32 Remaining = FShaderPipelineCache::NumPrecompilesRemaining();
	PeakRemaining = FMath::Max(PeakRemaining, Remaining);
	const float EstimatedProgress = PeakRemaining > 0
		? 1.0f - static_cast<float>(Remaining) / static_cast<float>(PeakRemaining)
		: 1.0f;
	const double CurrentTime = FPlatformTime::Seconds();
	const double SampleSeconds = CurrentTime - SpeedSampleStartedAt;
	if (SampleSeconds >= 1.0)
	{
		const uint32 Completed = SpeedSampleRemaining > Remaining ? SpeedSampleRemaining - Remaining : 0;
		RecentTasksPerSecond = static_cast<double>(Completed) / SampleSeconds;
		SpeedSampleStartedAt = CurrentTime;
		SpeedSampleRemaining = Remaining;
	}
	Progress.ExecuteIfBound(EstimatedProgress, Remaining, CurrentTime - WaitStartedAt, RecentTasksPerSecond);
	if (Remaining != 0)
	{
		if (Remaining != LastLoggedRemaining && CurrentTime - LastProgressLogAt >= 1.0)
		{
			UE_LOG(LogNoPhotos, Log, TEXT("[PSO] Preparing. Owner=%s Remaining=%u Elapsed=%.2fs"),
				*GetNameSafe(GetOwner()), Remaining, CurrentTime - WaitStartedAt);
			LastLoggedRemaining = Remaining;
			LastProgressLogAt = CurrentTime;
		}
		return;
	}

	UE_LOG(LogNoPhotos, Log, TEXT("[PSO] Pending work completed. Owner=%s Remaining=0 Elapsed=%.2fs"),
		*GetNameSafe(GetOwner()), CurrentTime - WaitStartedAt);
	WaitStartedAt = -1.0;
	FSimpleDelegate OnCompleted = MoveTemp(Completion);
	CancelWait();
	OnCompleted.ExecuteIfBound();
}

void UNPPSOPrecacheComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelWait();
	Super::EndPlay(EndPlayReason);
}
