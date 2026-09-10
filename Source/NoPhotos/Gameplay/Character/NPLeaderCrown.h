#pragma once

#include "Gameplay/AbilitySystem/GameplayCue/NPStatusVisualGameplayCue.h"
#include "NPLeaderCrown.generated.h"

class UStaticMeshComponent;
class UNiagaraComponent;

/** 1등 상태 동안 캐릭터 머리 위에서 트로피가 자전합니다. */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPLeaderCrown : public ANPStatusVisualGameplayCue
{
	GENERATED_BODY()

public:
	ANPLeaderCrown();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintSetter, Category="Leader|Visual")
	void SetVisibleToOwner(bool bNewVisibleToOwner);

protected:
	virtual void PrepareVisual() override;
	virtual void OnAppearTransitionStarted() override;
	virtual void ResetVisual() override;
	virtual void ApplyVisualScale() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Leader|Visual")
	TObjectPtr<UStaticMeshComponent> CrownMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Leader|Visual")
	TObjectPtr<UNiagaraComponent> AppearEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Leader|Visual", meta=(Units="cm"))
	float VisualHeight = 190.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Leader|Visual", meta=(Units="cm"))
	float AppearEffectZOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter=SetVisibleToOwner,
		Category="Leader|Visual", meta=(DisplayName="나에게 보이기"))
	bool bVisibleToOwner = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Leader|Visual", meta=(Units="deg/s"))
	float RotationSpeed = 30.0f;

private:
	FVector BaseCrownScale = FVector::OneVector;
	bool bBaseCrownScaleInitialized = false;
};
