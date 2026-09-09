#pragma once

#include "CoreMinimal.h"
#include "Gameplay/AbilitySystem/GameplayCue/NPAttachedGameplayCueActor.h"
#include "NPSpotlightBonusGameplayCue.generated.h"

class UNiagaraComponent;
class USoundAttenuation;
class USoundBase;

/** 스포트라이트 보상으로 유물 가치가 증가한 순간 캐릭터에 재생하는 일회성 연출입니다. */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPSpotlightBonusGameplayCue : public ANPAttachedGameplayCueActor
{
	GENERATED_BODY()

public:
	ANPSpotlightBonusGameplayCue();
	virtual bool GameplayCuePendingRemove() override;

protected:
	virtual bool OnExecute_Implementation(
		AActor* Target, const FGameplayCueParameters& Parameters) override;
	virtual bool Recycle() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Spotlight|Bonus")
	TObjectPtr<UNiagaraComponent> BonusEffect;

	/** 보상 캐릭터의 월드 위치에서 재생할 3D 사운드입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spotlight|Bonus|Sound")
	TObjectPtr<USoundBase> BonusSound;

	/** 보상음의 거리 감쇠 설정입니다. 지정하지 않으면 사운드 에셋의 설정을 사용합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spotlight|Bonus|Sound")
	TObjectPtr<USoundAttenuation> BonusSoundAttenuation;

	/** 0보다 크면 사운드 앞부분을 지정한 시간만큼 재생한 뒤 정지합니다. 0이면 끝까지 재생합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spotlight|Bonus|Sound",
		meta=(ClampMin="0.0", Units="s"))
	float BonusSoundPlaybackDuration = 0.0f;

private:
	UFUNCTION()
	void OnBonusEffectFinished(UNiagaraComponent* FinishedComponent);

	bool bIsPlaying = false;
};
