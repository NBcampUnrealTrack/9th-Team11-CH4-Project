#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NPShipGimmickSequenceManager.generated.h"

class ANPShipGimmickBase;
class FLifetimeProperty;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnShipGimmickSequenceProgressed, int32, CompletedSteps, int32, TotalSteps);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnShipGimmickSequenceReset);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnShipGimmickSequenceCompleted);

UCLASS(Blueprintable)
class NOPHOTOS_API ANPShipGimmickSequenceManager : public AActor
{
	GENERATED_BODY()

public:
	ANPShipGimmickSequenceManager();

	UFUNCTION(BlueprintPure, Category="Ship Gimmick|Sequence")
	int32 GetCompletedSteps() const { return CurrentStep; }

	UFUNCTION(BlueprintPure, Category="Ship Gimmick|Sequence")
	int32 GetTotalSteps() const { return GimmickSequence.Num(); }

	UFUNCTION(BlueprintPure, Category="Ship Gimmick|Sequence")
	bool IsSequenceCompleted() const { return bSequenceCompleted; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Ship Gimmick|Sequence")
	void ResetSequence();

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Ship Gimmick|Sequence", meta=(TitleProperty="GetName"))
	TArray<TObjectPtr<ANPShipGimmickBase>> GimmickSequence;

	/** Actor spawned once after the entire sequence is completed. Assign the high-value relic Blueprint here. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship Gimmick|Reward")
	TSubclassOf<AActor> TreasureClass;

	/** Relative to this manager actor. Z = 100 makes the reward visibly drop into the room. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship Gimmick|Reward")
	FVector TreasureSpawnOffset = FVector(0.0, 0.0, 100.0);

	UPROPERTY(BlueprintAssignable, Category="Ship Gimmick|Sequence")
	FOnShipGimmickSequenceProgressed OnProgressed;

	UPROPERTY(BlueprintAssignable, Category="Ship Gimmick|Sequence")
	FOnShipGimmickSequenceReset OnSequenceReset;

	UPROPERTY(BlueprintAssignable, Category="Ship Gimmick|Sequence")
	FOnShipGimmickSequenceCompleted OnSequenceCompleted;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintImplementableEvent, Category="Ship Gimmick|Sequence")
	void ReceiveSequenceProgressed(int32 CompletedSteps, int32 TotalSteps);

	UFUNCTION(BlueprintImplementableEvent, Category="Ship Gimmick|Sequence")
	void ReceiveSequenceReset();

	UFUNCTION(BlueprintImplementableEvent, Category="Ship Gimmick|Sequence")
	void ReceiveSequenceCompleted();

private:
	UFUNCTION()
	void HandleGimmickActivated(ANPShipGimmickBase* ActivatedGimmick);

	void SpawnSequenceTreasure();

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category="Ship Gimmick|Sequence", meta=(AllowPrivateAccess="true"))
	int32 CurrentStep = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category="Ship Gimmick|Sequence", meta=(AllowPrivateAccess="true"))
	bool bSequenceCompleted = false;
};
