#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "NPStatusVisualComponent.generated.h"

class UAbilitySystemComponent;
class UChildActorComponent;
class ANPStatusVisualManager;

/** GAS 리더 상태에 따른 왕관 표시를 관리합니다. */
UCLASS(ClassGroup=(Effects))
class NOPHOTOS_API UNPStatusVisualComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNPStatusVisualComponent();
	void Initialize(UAbilitySystemComponent* InAbilitySystem, UChildActorComponent* InLeaderCrown,
		ANPStatusVisualManager* InVisualManager);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleLeaderTagChanged(FGameplayTag Tag, int32 NewCount);

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;

	UPROPERTY(Transient)
	TObjectPtr<UChildActorComponent> LeaderCrown;

	UPROPERTY(Transient)
	TObjectPtr<ANPStatusVisualManager> VisualManager;

	FDelegateHandle LeaderTagHandle;
};
