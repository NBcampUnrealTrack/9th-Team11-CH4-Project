#include "Gameplay/Character/Component/NPStatusVisualComponent.h"

#include "AbilitySystemComponent.h"
#include "Components/ChildActorComponent.h"
#include "Core/GameplayTag/NPGameplayTags.h"
#include "Gameplay/Character/Component/NPControlReversalVisualComponent.h"

UNPStatusVisualComponent::UNPStatusVisualComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UNPStatusVisualComponent::Initialize(UAbilitySystemComponent* InAbilitySystem,
	UChildActorComponent* InLeaderCrown, UNPControlReversalVisualComponent* InControlReversalVisual)
{
	AbilitySystem = InAbilitySystem;
	LeaderCrown = InLeaderCrown;
	ControlReversalVisual = InControlReversalVisual;
	if (!IsValid(AbilitySystem))
	{
		return;
	}
	LeaderTagHandle = AbilitySystem->RegisterGameplayTagEvent(
		NPGameplayTags::State_Ranking_Leader, EGameplayTagEventType::NewOrRemoved).AddUObject(
			this, &ThisClass::HandleLeaderTagChanged);
	ControlTagHandle = AbilitySystem->RegisterGameplayTagEvent(
		NPGameplayTags::State_ControlsMirrored, EGameplayTagEventType::NewOrRemoved).AddUObject(
			this, &ThisClass::HandleControlTagChanged);
	HandleLeaderTagChanged(NPGameplayTags::State_Ranking_Leader,
		AbilitySystem->GetTagCount(NPGameplayTags::State_Ranking_Leader));
	HandleControlTagChanged(NPGameplayTags::State_ControlsMirrored,
		AbilitySystem->GetTagCount(NPGameplayTags::State_ControlsMirrored));
}

void UNPStatusVisualComponent::HandleLeaderTagChanged(FGameplayTag Tag, int32 NewCount)
{
	if (!IsValid(LeaderCrown))
	{
		return;
	}
	const bool bShowCrown = NewCount > 0 && GetNetMode() != NM_DedicatedServer;
	LeaderCrown->SetVisibility(bShowCrown, true);
	LeaderCrown->SetHiddenInGame(!bShowCrown, true);
	if (AActor* Crown = LeaderCrown->GetChildActor())
	{
		Crown->SetActorHiddenInGame(!bShowCrown);
		Crown->SetActorTickEnabled(bShowCrown);
	}
}

void UNPStatusVisualComponent::HandleControlTagChanged(FGameplayTag Tag, int32 NewCount)
{
	if (IsValid(ControlReversalVisual))
	{
		ControlReversalVisual->SetVisualActive(NewCount > 0 && GetNetMode() != NM_DedicatedServer);
	}
}

void UNPStatusVisualComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(AbilitySystem))
	{
		AbilitySystem->RegisterGameplayTagEvent(NPGameplayTags::State_Ranking_Leader,
			EGameplayTagEventType::NewOrRemoved).Remove(LeaderTagHandle);
		AbilitySystem->RegisterGameplayTagEvent(NPGameplayTags::State_ControlsMirrored,
			EGameplayTagEventType::NewOrRemoved).Remove(ControlTagHandle);
	}
	HandleLeaderTagChanged(NPGameplayTags::State_Ranking_Leader, 0);
	HandleControlTagChanged(NPGameplayTags::State_ControlsMirrored, 0);
	Super::EndPlay(EndPlayReason);
}
