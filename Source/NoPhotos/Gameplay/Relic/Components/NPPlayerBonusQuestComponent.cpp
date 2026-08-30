#include "Gameplay/Relic/Components/NPPlayerBonusQuestComponent.h"

#include "GameFramework/Actor.h"
#include "Gameplay/Relic/NPBaseRelic.h"
#include "Net/UnrealNetwork.h"

UNPPlayerBonusQuestComponent::UNPPlayerBonusQuestComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UNPPlayerBonusQuestComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(
		UNPPlayerBonusQuestComponent,
		AssignedQuestRelics,
		COND_OwnerOnly);
}

TArray<ANPBaseRelic*> UNPPlayerBonusQuestComponent::GetAssignedQuestRelics() const
{
	TArray<ANPBaseRelic*> Result;
	Result.Reserve(AssignedQuestRelics.Num());
	for (ANPBaseRelic* Relic : AssignedQuestRelics)
	{
		Result.Add(Relic);
	}
	return Result;
}

bool UNPPlayerBonusQuestComponent::IsAssignedQuestRelic(const ANPBaseRelic* Relic) const
{
	return IsValid(Relic) && AssignedQuestRelics.Contains(Relic);
}

bool UNPPlayerBonusQuestComponent::SetAssignedQuestRelics(
	const TArray<ANPBaseRelic*>& InQuestRelics)
{
	if (!IsOwnerAuthority() || InQuestRelics.IsEmpty())
	{
		return false;
	}

	TArray<TObjectPtr<ANPBaseRelic>> NewQuestRelics;
	NewQuestRelics.Reserve(InQuestRelics.Num());
	for (ANPBaseRelic* Relic : InQuestRelics)
	{
		if (!IsValid(Relic) || NewQuestRelics.Contains(Relic))
		{
			return false;
		}

		NewQuestRelics.Add(Relic);
	}

	AssignedQuestRelics = MoveTemp(NewQuestRelics);
	OnAssignedQuestRelicsChanged.Broadcast();
	return true;
}

void UNPPlayerBonusQuestComponent::ClearAssignedQuestRelics()
{
	if (!IsOwnerAuthority() || AssignedQuestRelics.IsEmpty())
	{
		return;
	}

	AssignedQuestRelics.Reset();
	OnAssignedQuestRelicsChanged.Broadcast();
}

void UNPPlayerBonusQuestComponent::OnRep_AssignedQuestRelics()
{
	OnAssignedQuestRelicsChanged.Broadcast();
}

bool UNPPlayerBonusQuestComponent::IsOwnerAuthority() const
{
	const AActor* Owner = GetOwner();
	return IsValid(Owner) && Owner->HasAuthority();
}
