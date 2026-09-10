#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "Gameplay/AbilitySystem/GameplayCue/NPAttachedGameplayCueActor.h"
#include "NPStatusVisualGameplayCue.generated.h"

/** 상태 연출 Gameplay Cue의 등장과 퇴장 전환을 담당합니다. */
UCLASS(Abstract, Blueprintable)
class NOPHOTOS_API ANPStatusVisualGameplayCue : public ANPAttachedGameplayCueActor
{
	GENERATED_BODY()

public:
	ANPStatusVisualGameplayCue();
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category="Status Visual")
	void RequestAppear();

	UFUNCTION(BlueprintCallable, Category="Status Visual")
	void RequestDisappear();

	UFUNCTION(BlueprintPure, Category="Status Visual")
	float GetVisualScaleMultiplier() const
	{
		return HasActorBegunPlay() ? ScaleMultiplier : 1.0f;
	}

protected:
	virtual bool WhileActive_Implementation(
		AActor* Target, const FGameplayCueParameters& Parameters) override;
	virtual bool OnRemove_Implementation(
		AActor* Target, const FGameplayCueParameters& Parameters) override;
	virtual bool Recycle() override;

	virtual void PrepareVisual() {}
	virtual void OnAppearTransitionStarted() {}
	virtual void ResetVisual() {}
	virtual void ApplyVisualScale() {}

	AActor* GetVisualTarget() const { return VisualTarget.Get(); }

	/** X축은 시간(초), Y축은 공통 스케일 배율입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Status Visual|Transition")
	FRuntimeFloatCurve AppearCurve;

	/** X축은 시간(초), Y축은 공통 스케일 배율입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Status Visual|Transition")
	FRuntimeFloatCurve DisappearCurve;

private:
	void BeginTransition(bool bAppearing);
	void FinishDisappear();
	const FRuntimeFloatCurve& GetTransitionCurve() const;

	TWeakObjectPtr<AActor> VisualTarget;
	float ScaleMultiplier = 0.0f;
	float StartScale = 0.0f;
	float Elapsed = 0.0f;
	float MinTime = 0.0f;
	float Duration = 0.0f;
	float CurveStartValue = 0.0f;
	float CurveEndValue = 0.0f;
	bool bAppearing = false;
	bool bTransitioning = false;
	bool bRemovalFinished = false;
};
