#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "NPAimableRelicImpactGameplayCue.generated.h"

class UNiagaraComponent;
class USceneComponent;

/** 조준 유물 Trace 적중 위치에서 Niagara를 재생하는 일회성 Cue입니다. */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPAimableRelicImpactGameplayCue : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	ANPAimableRelicImpactGameplayCue();
	virtual bool GameplayCuePendingRemove() override;

protected:
	/** Blueprint에서 Override한 뒤 Parent 호출 후 충돌음을 재생할 수 있습니다. */
	virtual bool OnExecute_Implementation(
		AActor* Target,
		const FGameplayCueParameters& Parameters) override;
	virtual bool Recycle() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Aimable Relic|Impact")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Blueprint 자식의 컴포넌트 기본값에서 충돌 Niagara System을 지정합니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Aimable Relic|Impact")
	TObjectPtr<UNiagaraComponent> ImpactEffect;

private:
	UFUNCTION()
	void HandleSystemFinished(UNiagaraComponent* FinishedComponent);

	bool bIsPlaying = false;
};
