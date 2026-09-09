#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "NPAimableRelicFireGameplayCue.generated.h"

class UNiagaraComponent;
class USceneComponent;

/** 조준 유물 발사 위치에서 총구 Niagara를 재생하는 일회성 Cue입니다. */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPAimableRelicFireGameplayCue : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	ANPAimableRelicFireGameplayCue();
	virtual bool GameplayCuePendingRemove() override;

protected:
	/** Blueprint에서 Override한 뒤 Parent 호출 후 발사음을 재생할 수 있습니다. */
	virtual bool OnExecute_Implementation(
		AActor* Target,
		const FGameplayCueParameters& Parameters) override;
	virtual bool Recycle() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Aimable Relic|Fire")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Blueprint 자식의 컴포넌트 기본값에서 총구 Niagara System을 지정합니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Aimable Relic|Fire")
	TObjectPtr<UNiagaraComponent> MuzzleEffect;

private:
	UFUNCTION()
	void HandleSystemFinished(UNiagaraComponent* FinishedComponent);

	bool bIsPlaying = false;
};
