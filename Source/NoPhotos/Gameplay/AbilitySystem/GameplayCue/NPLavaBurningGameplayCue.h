#pragma once

#include "CoreMinimal.h"
#include "Gameplay/AbilitySystem/GameplayCue/NPAttachedGameplayCueActor.h"
#include "NPLavaBurningGameplayCue.generated.h"

class UNiagaraComponent;

/** 용암 상태 동안 캐릭터 양옆에서 재생되는 불꽃 연출입니다. */
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

private:
	void SetLavaActive(bool bActive);
};
