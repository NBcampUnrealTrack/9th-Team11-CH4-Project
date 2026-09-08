#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "NPStatusVisualComponent.generated.h"

class UAbilitySystemComponent;
class UChildActorComponent;
class UNPControlReversalVisualComponent;

/** GAS 상태에 따른 왕관과 조작 반전 연출의 표시를 관리합니다. */
UCLASS(ClassGroup=(Effects))
class NOPHOTOS_API UNPStatusVisualComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNPStatusVisualComponent();
	void Initialize(UAbilitySystemComponent* InAbilitySystem, UChildActorComponent* InLeaderCrown,
		UNPControlReversalVisualComponent* InControlReversalVisual);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleLeaderTagChanged(FGameplayTag Tag, int32 NewCount);
	void HandleControlTagChanged(FGameplayTag Tag, int32 NewCount);

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;

	UPROPERTY(Transient)
	TObjectPtr<UChildActorComponent> LeaderCrown;

	UPROPERTY(Transient)
	TObjectPtr<UNPControlReversalVisualComponent> ControlReversalVisual;

	FDelegateHandle LeaderTagHandle;
	FDelegateHandle ControlTagHandle;
};
