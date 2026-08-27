#pragma once

#include "CoreMinimal.h"
#include "Components/DecalComponent.h"
#include "NPPhotoWorldFeedbackComponent.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UCameraShakeBase;

/**
 * 사진 촬영/피촬영 상태를 소유 Actor 발밑의 데칼 애니메이션으로 표시합니다.
 * 현재는 상대 Transform만 사용하며 바닥 탐색과 표면 Normal 정렬은 수행하지 않습니다.
 */
UCLASS(ClassGroup=(Photo), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPPhotoWorldFeedbackComponent : public UDecalComponent
{
	GENERATED_BODY()

public:
	UNPPhotoWorldFeedbackComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** 사진을 찍은 Actor용 데칼 효과를 처음부터 재생합니다. */
	UFUNCTION(BlueprintCallable, Category="Photo|World Feedback")
	void PlayPhotographerEffect();

	/** 사진에 찍힌 Actor용 데칼 효과를 처음부터 재생합니다. */
	UFUNCTION(BlueprintCallable, Category="Photo|World Feedback")
	void PlayPhotographedEffect();

	UFUNCTION(BlueprintCallable, Category="Photo|World Feedback")
	void StopFeedback();

	UFUNCTION(BlueprintPure, Category="Photo|World Feedback")
	bool IsFeedbackPlaying() const { return bIsPlaying; }

protected:
	/** 사진을 찍은 Actor의 발밑에 표시할 데칼 재질입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Photo|World Feedback|Material")
	TObjectPtr<UMaterialInterface> PhotographerMaterial;

	/** 사진에 찍힌 Actor의 발밑에 표시할 데칼 재질입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Photo|World Feedback|Material")
	TObjectPtr<UMaterialInterface> PhotographedMaterial;

	/** 데칼 재질에서 투명도를 제어하는 Scalar Parameter 이름입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Photo|World Feedback|Material")
	FName OpacityParameterName = TEXT("Opacity");

	/** 최대 크기입니다. X는 투영 깊이이고 Y/Z가 바닥에 보이는 크기입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Photo|World Feedback|Animation")
	FVector MaximumDecalSize = FVector(32.0f, 80.0f, 80.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Photo|World Feedback|Animation", meta=(ClampMin="0.0"))
	float GrowDuration = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Photo|World Feedback|Animation", meta=(ClampMin="0.0"))
	float HoldDuration = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Photo|World Feedback|Animation", meta=(ClampMin="0.0"))
	float ShrinkDuration = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Photo|World Feedback|Animation", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MaximumOpacity = 1.0f;

	/** 피촬영자 자신의 로컬 화면에서만 재생할 짧은 Camera Shake입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Photo|World Feedback|Camera Shake")
	TSubclassOf<UCameraShakeBase> PhotographedCameraShake;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Photo|World Feedback|Camera Shake", meta=(ClampMin="0.0"))
	float PhotographedCameraShakeScale = 1.0f;

private:
	void PlayFeedback(UMaterialInterface* FeedbackMaterial);
	void PlayLocalPhotographedCameraShake();
	void ApplyAnimationState(float SizeAlpha, float OpacityAlpha);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;

	float ElapsedTime = 0.0f;
	bool bIsPlaying = false;
};
