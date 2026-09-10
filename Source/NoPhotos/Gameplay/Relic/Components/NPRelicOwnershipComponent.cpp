#include "Gameplay/Relic/Components/NPRelicOwnershipComponent.h"

#include "Core/NPPlayerState.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/Character/Component/NPStablePhysicsGrabComponent.h"

UNPRelicOwnershipComponent::UNPRelicOwnershipComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

bool UNPRelicOwnershipComponent::HasServerAuthority() const
{
	const AActor* OwnerActor = GetOwner();
	return IsValid(OwnerActor) && OwnerActor->HasAuthority();
}

void UNPRelicOwnershipComponent::RegisterGrabber(
	UNPStablePhysicsGrabComponent* GrabComponent,
	ANPPlayerState* PlayerState)
{
	if (!HasServerAuthority() || !IsValid(GrabComponent) || !IsValid(PlayerState))
	{
		return;
	}

	const TWeakObjectPtr<ANPPlayerState>* ExistingOwner =
		ActiveGrabbers.Find(GrabComponent);
	if (ExistingOwner && ExistingOwner->Get() == PlayerState)
	{
		return;
	}

	ActiveGrabbers.FindOrAdd(GrabComponent) = PlayerState;
	UpdateReplicationOwner();
	OnOwnershipChanged.Broadcast(this);
}

void UNPRelicOwnershipComponent::UnregisterGrabber(
	UNPStablePhysicsGrabComponent* GrabComponent)
{
	if (!HasServerAuthority() || !IsValid(GrabComponent))
	{
		return;
	}

	if (ActiveGrabbers.Remove(GrabComponent) == 0)
	{
		return;
	}

	UpdateReplicationOwner();
	OnOwnershipChanged.Broadcast(this);
}

void UNPRelicOwnershipComponent::GetCurrentOwners(
	TArray<ANPPlayerState*>& OutOwners) const
{
	OutOwners.Reset();
	TSet<ANPPlayerState*> UniqueOwners;

	for (const TPair<
		TWeakObjectPtr<UNPStablePhysicsGrabComponent>,
		TWeakObjectPtr<ANPPlayerState>>& Pair : ActiveGrabbers)
	{
		if (Pair.Key.IsValid() && Pair.Value.IsValid())
		{
			UniqueOwners.Add(Pair.Value.Get());
		}
	}

	OutOwners.Reserve(UniqueOwners.Num());
	for (ANPPlayerState* Owner : UniqueOwners)
	{
		OutOwners.Add(Owner);
	}
}

int32 UNPRelicOwnershipComponent::GetCurrentOwnerCount() const
{
	TArray<ANPPlayerState*> Owners;
	GetCurrentOwners(Owners);
	return Owners.Num();
}

void UNPRelicOwnershipComponent::ReleaseAllGrabbers()
{
	if (!HasServerAuthority())
	{
		return;
	}

	TArray<TWeakObjectPtr<UNPStablePhysicsGrabComponent>> Grabbers;
	ActiveGrabbers.GenerateKeyArray(Grabbers);
	for (const TWeakObjectPtr<UNPStablePhysicsGrabComponent>& Grabber : Grabbers)
	{
		if (Grabber.IsValid())
		{
			Grabber->SetGrabRequested(false);
		}
	}
}

void UNPRelicOwnershipComponent::ClearOwnership()
{
	if (HasServerAuthority())
	{
		const bool bOwnershipChanged = !ActiveGrabbers.IsEmpty();
		ActiveGrabbers.Reset();
		UpdateReplicationOwner();
		if (bOwnershipChanged)
		{
			OnOwnershipChanged.Broadcast(this);
		}
	}
}

void UNPRelicOwnershipComponent::UpdateReplicationOwner()
{
	AActor* Relic = GetOwner();
	if (!Relic)
	{
		return;
	}

	TArray<ANPPlayerState*> Owners;
	GetCurrentOwners(Owners);
	APlayerController* NewReplicationOwner = Owners.Num() == 1
		? Owners[0]->GetPlayerController()
		: nullptr;
	if (Relic->GetOwner() == NewReplicationOwner)
	{
		return;
	}

	Relic->SetOwner(NewReplicationOwner);
	Relic->ForceNetUpdate();
}
