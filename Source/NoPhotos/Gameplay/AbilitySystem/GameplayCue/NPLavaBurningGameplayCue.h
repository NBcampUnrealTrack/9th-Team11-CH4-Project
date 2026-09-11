#pragma once

#include "CoreMinimal.h"
#include "Gameplay/AbilitySystem/GameplayCue/NPAttachedGameplayCueActor.h"
#include "NPLavaBurningGameplayCue.generated.h"

class UAudioComponent;
class UNiagaraComponent;

/** 용암 상태 동안 캐릭터 몸에서 재생되는 불꽃과 사운드 연출입니다. */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPLavaBurningGameplayCue : public ANPAttachedGameplayCueActor
{
	GENERATED_BODY()

public:
	ANPLavaBurningGameplayCue();

protected:
	virtual bool WhileActive_Implementation(
		AActor* Target, const FGameplayCueParameters& Parameters) override;
	virtual bool OnRemove_Implementation(
		AActor* Target, const FGameplayCueParameters& Parameters) override;
	virtual bool Recycle() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Lava|Visual")
	TObjectPtr<UNiagaraComponent> LavaFireLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Lava|Visual")
	TObjectPtr<UNiagaraComponent> LavaFireRight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Lava|Sound")
	TObjectPtr<UAudioComponent> LavaAudio;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Lava|Sound", meta=(ClampMin="0.0", Units="s"))
	float LavaSoundStartTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Lava|Sound", meta=(ClampMin="0.01", Units="s"))
	float LavaSoundDuration = 0.5f;

private:
	void SetLavaActive(bool bActive);
	void RestartLavaSound();
	void StopLavaSound();

	FTimerHandle LavaSoundStopTimerHandle;
};
