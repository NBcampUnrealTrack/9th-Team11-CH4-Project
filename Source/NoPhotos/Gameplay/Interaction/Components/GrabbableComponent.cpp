#include "GrabbableComponent.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UGrabbableComponent::UGrabbableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UGrabbableComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UGrabbableComponent, bAdditionalGrabLocked);
}

void UGrabbableComponent::SetGrabEnabled(bool bEnabled)
{
	bGrabEnabled = bEnabled;
	if (!bGrabEnabled)
	{
		ForceReleaseAllGrabs();
	}
}

void UGrabbableComponent::ForceReleaseAllGrabs()
{
	if (ActiveGrabCount > 0)
	{
		OnForceReleaseAllGrabs.Broadcast();
	}
}

bool UGrabbableComponent::AcquireAdditionalGrabLock()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return false;
	}

	++AdditionalGrabLockCount;
	bAdditionalGrabLocked = true;
	OwnerActor->ForceNetUpdate();
	return true;
}

void UGrabbableComponent::ReleaseAdditionalGrabLock()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor
		|| !OwnerActor->HasAuthority()
		|| AdditionalGrabLockCount <= 0)
	{
		return;
	}

	--AdditionalGrabLockCount;
	if (AdditionalGrabLockCount == 0)
	{
		bAdditionalGrabLocked = false;
		OwnerActor->ForceNetUpdate();
	}
}

UPrimitiveComponent* UGrabbableComponent::ResolveGrabTarget(
	UPrimitiveComponent* DetectedComponent) const
{
	if (AActor* Owner = GetOwner())
	{
		if (UPrimitiveComponent* RootPrimitive =
			Cast<UPrimitiveComponent>(Owner->GetRootComponent()))
		{
			return RootPrimitive;
		}
	}

	return DetectedComponent;
}

void UGrabbableComponent::NotifyGrabStarted(UPrimitiveComponent* GrabbedComponent)
{
	if (!CanBeGrabbed())
	{
		return;
	}

	++ActiveGrabCount;
	bIsGrabbed = true;
	OnActiveGrabCountChanged.Broadcast(ActiveGrabCount);
	if (ActiveGrabCount == 1)
	{
		CurrentLinearGrabForce = FVector::ZeroVector;
		CurrentAngularGrabForce = FVector::ZeroVector;
		OnGrabStarted.Broadcast(GrabbedComponent);
	}
}

void UGrabbableComponent::NotifyGrabForce(
	const FVector& LinearForce,
	const FVector& AngularForce,
	float IntentForceAlignment)
{
	if (!bIsGrabbed)
	{
		return;
	}

	CurrentLinearGrabForce = LinearForce;
	CurrentAngularGrabForce = AngularForce;
	OnGrabForceUpdated.Broadcast(
		CurrentLinearGrabForce,
		CurrentAngularGrabForce,
		IntentForceAlignment);
}

void UGrabbableComponent::NotifyGrabEnded()
{
	if (ActiveGrabCount <= 0)
	{
		return;
	}

	--ActiveGrabCount;
	bIsGrabbed = ActiveGrabCount > 0;
	OnActiveGrabCountChanged.Broadcast(ActiveGrabCount);
	if (bIsGrabbed)
	{
		return;
	}

	CurrentLinearGrabForce = FVector::ZeroVector;
	CurrentAngularGrabForce = FVector::ZeroVector;
	OnGrabEnded.Broadcast();
}
