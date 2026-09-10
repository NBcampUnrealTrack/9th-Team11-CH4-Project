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

	/** 배정된 목표 유물을 제출했을 때 해당 플레이어에게 추가할 점수입니다. */
	UFUNCTION(BlueprintPure, Category="Player Bonus Quest")
	int32 GetQuestRelicDeliveryBonusScore() const { return QuestRelicDeliveryBonusScore; }

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

	/** PlayerController Blueprint의 컴포넌트 기본값에서 조정할 수 있습니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Player Bonus Quest",
		meta=(AllowPrivateAccess="true", ClampMin="0", UIMin="0"))
	int32 QuestRelicDeliveryBonusScore = 100;

	UPROPERTY(ReplicatedUsing=OnRep_AssignedQuestRelics, VisibleInstanceOnly, BlueprintReadOnly, Category="Player Bonus Quest", meta=(AllowPrivateAccess="true"))
	TArray<TObjectPtr<ANPBaseRelic>> AssignedQuestRelics;
};
