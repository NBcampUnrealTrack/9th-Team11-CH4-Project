#pragma once

#include "CoreMinimal.h"
#include "Gameplay/AbilitySystem/GameplayCue/NPAttachedGameplayCueActor.h"
#include "NPPhotoWorldFeedbackGameplayCue.generated.h"

class UCameraShakeBase;
class UDecalComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;

/** 사진 촬영 또는 피촬영 순간 대상 발밑에 표시되는 일회성 데칼 연출입니다. */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPPhotoWorldFeedbackGameplayCue : public ANPAttachedGameplayCueActor
{
	GENERATED_BODY()

public:
	ANPPhotoWorldFeedbackGameplayCue();
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual bool OnExecute_Implementation(
		AActor* Target, const FGameplayCueParameters& Parameters) override;
	virtual bool Recycle() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Photo|World Feedback")
	TObjectPtr<UDecalComponent> FeedbackDecal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Photo|World Feedback|Material")
	TObjectPtr<UMaterialInterface> FeedbackMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Photo|World Feedback|Material")
	FName OpacityParameterName = TEXT("Opacity");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Photo|World Feedback|Animation")
	FVector MaximumDecalSize = FVector(32.0f, 80.0f, 80.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Photo|World Feedback|Animation",
		meta=(ClampMin="0.0"))
	float GrowDuration = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Photo|World Feedback|Animation",
		meta=(ClampMin="0.0"))
	float HoldDuration = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Photo|World Feedback|Animation",
		meta=(ClampMin="0.0"))
	float ShrinkDuration = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Photo|World Feedback|Animation",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float MaximumOpacity = 1.0f;

	/** 지정된 경우 대상 Pawn을 조종하는 로컬 플레이어에게만 재생합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Photo|World Feedback|Camera Shake")
	TSubclassOf<UCameraShakeBase> LocalCameraShake;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Photo|World Feedback|Camera Shake",
		meta=(ClampMin="0.0"))
	float LocalCameraShakeScale = 1.0f;

private:
	void ResetFeedback();
	void PlayLocalCameraShake(AActor* Target) const;
	void ApplyAnimationState(float SizeAlpha, float OpacityAlpha);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;

	float ElapsedTime = 0.0f;
	bool bIsPlaying = false;
};
