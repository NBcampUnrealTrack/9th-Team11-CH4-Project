#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NPPlayerBonusQuestComponent.generated.h"

class ANPBaseRelic;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNPOnPlayerBonusQuestChanged);
UCLASS(ClassGroup=(Relic), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPPlayerBonusQuestComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNPPlayerBonusQuestComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Player Bonus Quest")
	TArray<ANPBaseRelic*> GetAssignedQuestRelics() const;

	UFUNCTION(BlueprintPure, Category="Player Bonus Quest")
	bool IsAssignedQuestRelic(const ANPBaseRelic* Relic) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Player Bonus Quest")
	bool SetAssignedQuestRelics(const TArray<ANPBaseRelic*>& InQuestRelics);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Player Bonus Quest")
	void ClearAssignedQuestRelics();

	UPROPERTY(BlueprintAssignable, Category="Player Bonus Quest")
	FNPOnPlayerBonusQuestChanged OnAssignedQuestRelicsChanged;

private:
	UFUNCTION()
	void OnRep_AssignedQuestRelics();

	bool IsOwnerAuthority() const;

	UPROPERTY(ReplicatedUsing=OnRep_AssignedQuestRelics, VisibleInstanceOnly, BlueprintReadOnly, Category="Player Bonus Quest", meta=(AllowPrivateAccess="true"))
	TArray<TObjectPtr<ANPBaseRelic>> AssignedQuestRelics;
};
