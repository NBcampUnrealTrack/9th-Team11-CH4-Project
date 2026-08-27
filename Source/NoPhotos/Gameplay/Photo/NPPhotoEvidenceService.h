#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Photo/NPPhotoEvidenceTypes.h"
#include "UObject/Object.h"
#include "NPPhotoEvidenceService.generated.h"

class AActor;
class ANPMainGameMode;
class APawn;

/** 서버 월드 상태를 이용해 사진 속 도둑과 Relic의 유효성을 판정합니다. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class NOPHOTOS_API UNPPhotoEvidenceService : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(ANPMainGameMode* InOwningGameMode);
	FNPPhotoEvidenceResult EvaluatePhoto(const FNPPhotoCaptureRequest& Request);
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Photo|Validation", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MinimumThiefVisibility = 0.5f;

	/** 유물 보유 여부와 무관한 플레이어 촬영 판정의 최소 가시율입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Photo|Validation", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MinimumPlayerCaptureVisibility = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Photo|Validation", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MinimumRelicVisibility = 0.5f;

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

	TWeakObjectPtr<ANPMainGameMode> OwningGameMode;
	TMap<TWeakObjectPtr<APlayerController>, double> LastCaptureTimes;
};
