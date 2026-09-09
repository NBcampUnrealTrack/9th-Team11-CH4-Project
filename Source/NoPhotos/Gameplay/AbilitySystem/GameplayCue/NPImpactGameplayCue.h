#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "NPImpactGameplayCue.generated.h"

class UNiagaraComponent;
class USceneComponent;

UCLASS(Blueprintable)
class NOPHOTOS_API ANPImpactGameplayCue : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	ANPImpactGameplayCue();
	virtual bool GameplayCuePendingRemove() override;

protected:
	virtual bool OnExecute_Implementation(
		AActor* Target, const FGameplayCueParameters& Parameters) override;
	virtual bool Recycle() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Impact")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Impact")
	TObjectPtr<UNiagaraComponent> ImpactEffect;

private:
	UFUNCTION()
	void OnImpactFinished(UNiagaraComponent* FinishedComponent);

	bool bIsPlaying = false;
};
