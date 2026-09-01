#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NPRelicSetup.generated.h"

class ANPBaseRelic;
class FLifetimeProperty;
class UNPRelicGimmickComponent;

UCLASS(Blueprintable)
class NOPHOTOS_API ANPRelicSetup : public AActor
{
	GENERATED_BODY()

public:
	ANPRelicSetup();
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual bool PrepareRelicSetup();
	virtual void CollectGimmicks();
	void CollectGimmicksFromActor(AActor* GimmickActor);
	virtual void RefreshRelicLock();
	bool AreAllGimmicksCompleted() const;

	UPROPERTY(
		EditInstanceOnly,
		Replicated,
		BlueprintReadOnly,
		Category="Relic Setup")
	TObjectPtr<ANPBaseRelic> Relic;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Relic Setup")
	TArray<TObjectPtr<AActor>> GimmickActors;

private:
	void HandleGimmickCompleted();

	UPROPERTY(Transient, VisibleInstanceOnly, Category="Relic Setup|Debug")
	TArray<TObjectPtr<UNPRelicGimmickComponent>> Gimmicks;
};
