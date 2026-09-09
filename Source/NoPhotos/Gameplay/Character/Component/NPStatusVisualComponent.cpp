#include "Gameplay/Character/Component/NPStatusVisualComponent.h"

#include "AbilitySystemComponent.h"
#include "Components/ChildActorComponent.h"
#include "Core/GameplayTag/NPGameplayTags.h"
#include "Gameplay/Character/NPStatusVisualManager.h"

UNPStatusVisualComponent::UNPStatusVisualComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UNPStatusVisualComponent::Initialize(UAbilitySystemComponent* InAbilitySystem,
	UChildActorComponent* InLeaderCrown, ANPStatusVisualManager* InVisualManager)
{
	AbilitySystem = InAbilitySystem;
	LeaderCrown = InLeaderCrown;
	VisualManager = InVisualManager;
	if (IsValid(VisualManager))
	{
		VisualManager->InitializeLeaderCrown(LeaderCrown);
	}
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
	if (IsValid(VisualManager))
	{
		VisualManager->RequestVisual(ENPStatusVisualType::Leader, bShowCrown);
		return;
	}
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
	if (IsValid(LeaderCrown))
	{
		LeaderCrown->SetVisibility(false, true);
		LeaderCrown->SetHiddenInGame(true, true);
		if (AActor* Crown = LeaderCrown->GetChildActor())
		{
			Crown->SetActorHiddenInGame(true);
			Crown->SetActorTickEnabled(false);
		}
	}
	Super::EndPlay(EndPlayReason);
}
