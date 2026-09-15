#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Photo/NPPhotoEvidenceTypes.h"
#include "UObject/Object.h"
#include "NPPhotoEvidenceService.generated.h"

class AActor;
class ANPMainGameMode;
class ANPReplicatedStablePhysicsPawn;
class APawn;

/** 사진 속 유물 보유 플레이어와 사진 반응 대상의 유효성을 판정합니다. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class NOPHOTOS_API UNPPhotoEvidenceService : public UObject
{
	GENERATED_BODY()

public:
	UNPPhotoEvidenceService();

	void Initialize(ANPMainGameMode* InOwningGameMode);
	FNPPhotoEvidenceResult EvaluatePhoto(const FNPPhotoCaptureRequest& Request);
	void SetMinimumVisibleHeadSampleCount(int32 InSampleCount);
	int32 GetMinimumVisibleHeadSampleCount() const { return MinimumVisibleHeadSampleCount; }
	bool IsRelicHolderCapturable(
		const FNPPhotoCaptureRequest& Request,
		ANPReplicatedStablePhysicsPawn* TargetPawn,
		APawn* PhotographerPawn,
		AActor*& OutHeldRelic,
		int32 RequiredVisibleHeadSampleCount = INDEX_NONE) const;
	virtual UWorld* GetWorld() const override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Photo|Validation", meta=(ClampMin="0.0"))
	float MaximumCaptureDistance = 2000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Photo|Validation", meta=(ClampMin="1.0", ClampMax="179.0"))
	float HorizontalFOV = 60.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Photo|Validation", meta=(ClampMin="0.1"))
	float CaptureAspectRatio = 1.777778f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Photo|Validation", meta=(ClampMin="0.1", ClampMax="1.0"))
	float FOVAcceptanceScale = 0.9f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Photo|Validation")
	TArray<FName> HeadVisibilitySampleNames;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Photo|Validation", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MinimumReactiveTargetVisibility = 0.5f;

	/** 요청 카메라가 서버 Pawn 원점에서 떨어질 수 있는 최대 거리입니다. 3인칭 Spring Arm 길이를 포함해야 합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Photo|Security", meta=(ClampMin="0.0"))
	float MaximumCameraDistanceFromPawn = 700.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Photo|Security", meta=(ClampMin="0.0", ClampMax="180.0"))
	float MaximumCameraDirectionError = 25.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Photo|Security", meta=(ClampMin="0.0"))
	float ServerCaptureCooldown = 5.0f;

private:
	int32 MinimumVisibleHeadSampleCount = 2;

	bool ValidateRequest(
		const FNPPhotoCaptureRequest& Request,
		FNPPhotoEvidenceResult& OutResult);
	bool IsInsideCameraFOV(
		const FNPPhotoCaptureRequest& Request,
		const FVector& TargetLocation) const;
	float CalculateActorVisibility(
		const FNPPhotoCaptureRequest& Request,
		AActor* TargetActor,
		APawn* PhotographerPawn) const;
	void BuildActorSamplePoints(AActor* TargetActor, TArray<FVector>& OutPoints) const;
	int32 CountVisibleHeadSamples(
		const FNPPhotoCaptureRequest& Request,
		ANPReplicatedStablePhysicsPawn* TargetPawn,
		APawn* PhotographerPawn) const;

	TWeakObjectPtr<ANPMainGameMode> OwningGameMode;
	TMap<TWeakObjectPtr<APlayerController>, double> LastCaptureTimes;
};
