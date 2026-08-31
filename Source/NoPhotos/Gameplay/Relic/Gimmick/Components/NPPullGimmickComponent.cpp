#include "Gameplay/Relic/Gimmick/Components/NPPullGimmickComponent.h"

#include "GameFramework/Actor.h"
#include "Gameplay/Interaction/Components/GrabbableComponent.h"
#include "NoPhotos.h"

UNPPullGimmickComponent::UNPPullGimmickComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UNPPullGimmickComponent::NotifyPullFinished()
{
	if (!GetOwner()
		|| !GetOwner()->HasAuthority()
		|| !bIsPullPresentationPlaying)
	{
		return;
	}

	bIsPullPresentationPlaying = false;
	UE_LOG(
		LogNoPhotos,
		Log,
		TEXT("[%s] Pull presentation finished. Count=%d/%d"),
		*GetNameSafe(GetOwner()),
		CurrentPullCount,
		RequiredPullCount);
	if (CurrentPullCount >= RequiredPullCount)
	{
		CompleteGimmick();
	}
}

void UNPPullGimmickComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	GrabbableComponent = GetOwner()->FindComponentByClass<UGrabbableComponent>();
	if (!GrabbableComponent)
	{
		UE_LOG(
			LogNoPhotos,
			Warning,
			TEXT("[%s] PullGimmick requires a GrabbableComponent."),
			*GetNameSafe(GetOwner()));
		return;
	}

	GrabbableComponent->OnGrabStarted.AddUObject(
		this,
		&UNPPullGimmickComponent::HandleGrabStarted);
	GrabbableComponent->OnGrabForceUpdated.AddUObject(
		this,
		&UNPPullGimmickComponent::HandleGrabForceUpdated);
	GrabbableComponent->OnGrabEnded.AddUObject(
		this,
		&UNPPullGimmickComponent::HandleGrabEnded);
}

void UNPPullGimmickComponent::HandleGrabStarted(UPrimitiveComponent*)
{
	++PullAttemptCount;
	bPullForceExceeded = false;
	CurrentAttemptMaxPullForce = 0.0f;
	CurrentAttemptMaxLinearForce = 0.0f;

	UE_LOG(
		LogNoPhotos,
		Log,
		TEXT("[%s] Pull attempt %d started."),
		*GetNameSafe(GetOwner()),
		PullAttemptCount);
}

void UNPPullGimmickComponent::HandleGrabForceUpdated(
	const FVector& LinearForce,
	const FVector&,
	float IntentForceAlignment)
{
	if (IsCompleted() || PullDirection.IsNearlyZero())
	{
		return;
	}

	const FVector WorldPullDirection = PullDirection.GetSafeNormal();
	const float PullForce = FVector::DotProduct(
		LinearForce,
		WorldPullDirection);
	CurrentAttemptMaxPullForce = FMath::Max(
		CurrentAttemptMaxPullForce,
		PullForce);
	CurrentAttemptMaxLinearForce = FMath::Max(
		CurrentAttemptMaxLinearForce,
		LinearForce.Size());
	const bool bExceedsThreshold =
		PullForce >= PullForceThreshold
		&& IntentForceAlignment >= MinimumIntentAlignment;

	if (bExceedsThreshold
		&& !bPullForceExceeded
		&& !bIsPullPresentationPlaying)
	{
		++CurrentPullCount;
		bIsPullPresentationPlaying = true;
		UE_LOG(
			LogNoPhotos,
			Log,
			TEXT("[%s] Pull accepted. Attempt=%d Force=%.1f Count=%d/%d"),
			*GetNameSafe(GetOwner()),
			PullAttemptCount,
			PullForce,
			CurrentPullCount,
			RequiredPullCount);
		if (OnPullSucceeded.IsBound())
		{
			OnPullSucceeded.Broadcast(
				CurrentPullCount,
				RequiredPullCount);
		}
		else
		{
			NotifyPullFinished();
		}
	}

	bPullForceExceeded = bExceedsThreshold;
}

void UNPPullGimmickComponent::HandleGrabEnded()
{
	UE_LOG(
		LogNoPhotos,
		Log,
		TEXT("[%s] Pull attempt %d ended. MaxPullForce=%.1f MaxLinearForce=%.1f Threshold=%.1f Count=%d/%d"),
		*GetNameSafe(GetOwner()),
		PullAttemptCount,
		CurrentAttemptMaxPullForce,
		CurrentAttemptMaxLinearForce,
		PullForceThreshold,
		CurrentPullCount,
		RequiredPullCount);

	bPullForceExceeded = false;
}
