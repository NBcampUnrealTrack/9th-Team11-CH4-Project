#include "Gameplay/Character/Component/NPStatusVisualComponent.h"

#include "AbilitySystemComponent.h"
#include "Components/ChildActorComponent.h"
#include "Core/GameplayTag/NPGameplayTags.h"

UNPStatusVisualComponent::UNPStatusVisualComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UNPStatusVisualComponent::Initialize(UAbilitySystemComponent* InAbilitySystem,
	UChildActorComponent* InLeaderCrown)
{
	AbilitySystem = InAbilitySystem;
	LeaderCrown = InLeaderCrown;
	if (!IsValid(AbilitySystem))
	{
		return;
	}
	LeaderTagHandle = AbilitySystem->RegisterGameplayTagEvent(
		NPGameplayTags::State_Ranking_Leader, EGameplayTagEventType::NewOrRemoved).AddUObject(
			this, &ThisClass::HandleLeaderTagChanged);
	HandleLeaderTagChanged(NPGameplayTags::State_Ranking_Leader,
		AbilitySystem->GetTagCount(NPGameplayTags::State_Ranking_Leader));
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

void UNPStatusVisualComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(AbilitySystem))
	{
		AbilitySystem->RegisterGameplayTagEvent(NPGameplayTags::State_Ranking_Leader,
			EGameplayTagEventType::NewOrRemoved).Remove(LeaderTagHandle);
	}
	HandleLeaderTagChanged(NPGameplayTags::State_Ranking_Leader, 0);
	Super::EndPlay(EndPlayReason);
}
