#include "Gameplay/Relic/Gimmick/Components/NPPullGimmickComponent.h"

#include "GameFramework/Actor.h"
#include "Gameplay/Interaction/Components/GrabbableComponent.h"
#include "Net/UnrealNetwork.h"
#include "NoPhotos.h"

UNPPullGimmickComponent::UNPPullGimmickComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UNPPullGimmickComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UNPPullGimmickComponent, CurrentPullCount);
}

void UNPPullGimmickComponent::NotifyPullFinished()
{
	if (!GetOwner() || !bIsPullPresentationPlaying)
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
}

void UNPPullGimmickComponent::OnRep_CurrentPullCount()
{
	if (CurrentPullCount <= 0)
	{
		return;
	}

	bIsPullPresentationPlaying = true;
	UE_LOG(
		LogNoPhotos,
		Log,
		TEXT("[%s] Pull state received. Role=%s Count=%d/%d"),
		*GetNameSafe(GetOwner()),
		GetOwner() && GetOwner()->HasAuthority()
			? TEXT("Authority")
			: TEXT("Client"),
		CurrentPullCount,
		RequiredPullCount);
	OnPullSucceeded.Broadcast(CurrentPullCount, RequiredPullCount);
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
	if (IsCompleted())
	{
		return;
	}

	const float PullForce = PullDirection.IsNearlyZero()
		? LinearForce.Size()
		: FVector::DotProduct(LinearForce, PullDirection.GetSafeNormal());
	CurrentAttemptMaxPullForce = FMath::Max(
		CurrentAttemptMaxPullForce,
		PullForce);
	CurrentAttemptMaxLinearForce = FMath::Max(
		CurrentAttemptMaxLinearForce,
		LinearForce.Size());
	const bool bExceedsThreshold =
		PullForce >= PullForceThreshold
		&& IntentForceAlignment >= MinimumIntentAlignment;

	if (bExceedsThreshold && !bPullForceExceeded)
	{
		++CurrentPullCount;
		UE_LOG(
			LogNoPhotos,
			Log,
			TEXT("[%s] Pull accepted. Attempt=%d Force=%.1f Count=%d/%d"),
			*GetNameSafe(GetOwner()),
			PullAttemptCount,
			PullForce,
			CurrentPullCount,
			RequiredPullCount);

		// RepNotify는 서버에서 자동 호출되지 않으므로 서버 연출도 같은
		// 경로를 사용하도록 직접 호출합니다. 기믹 완료 판정은 연출
		// callback과 분리하여 서버가 즉시 확정합니다.
		OnRep_CurrentPullCount();
		GetOwner()->ForceNetUpdate();
		if (CurrentPullCount >= RequiredPullCount)
		{
			CompleteGimmick();
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
