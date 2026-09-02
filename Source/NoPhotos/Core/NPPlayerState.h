#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "NPPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNPOnPlayerScoreChanged);

UCLASS()
class NOPHOTOS_API ANPPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Score")
	void AddScore(int32 ScoreToAdd);

	UFUNCTION(BlueprintPure, Category = "Score")
	int32 GetPlayerScore() const;

	void ResetPlayerScore();

	UPROPERTY(BlueprintAssignable, Category = "Score")
	FNPOnPlayerScoreChanged OnPlayerScoreChanged;

private:
	UFUNCTION()
	void OnRep_PlayerScore();

	UPROPERTY(ReplicatedUsing = OnRep_PlayerScore)
	int32 PlayerScore = 0;
};
