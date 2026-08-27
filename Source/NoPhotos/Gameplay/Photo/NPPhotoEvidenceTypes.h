#pragma once

#include "CoreMinimal.h"
#include "NPPhotoEvidenceTypes.generated.h"

class AActor;
class APlayerController;
class APlayerState;

UENUM(BlueprintType)
enum class ENPPhotoEvidenceFailureReason : uint8
{
	None,
	InvalidPhotographer,
	PhotographerIsGrabbing,
	CaptureOnCooldown,
	InvalidCamera,
	NoValidEvidence
};

/** 서버 내부에서 사용하는 검증된 촬영 요청입니다. */
USTRUCT()
struct NOPHOTOS_API FNPPhotoCaptureRequest
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<APlayerController> Photographer = nullptr;

	UPROPERTY()
	FVector CameraLocation = FVector::ZeroVector;

	UPROPERTY()
	FVector CameraForward = FVector::ForwardVector;

	UPROPERTY()
	uint16 CaptureSequence = 0;
};

/** 사진 한 장에 대한 서버의 상세 판정 결과입니다. */
USTRUCT(BlueprintType)
struct NOPHOTOS_API FNPPhotoEvidenceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Photo")
	int32 CaptureSequence = 0;

	UPROPERTY(BlueprintReadOnly, Category="Photo")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category="Photo")
	ENPPhotoEvidenceFailureReason FailureReason = ENPPhotoEvidenceFailureReason::None;

	UPROPERTY(BlueprintReadOnly, Category="Photo")
	TObjectPtr<APlayerState> Photographer = nullptr;

	UPROPERTY(BlueprintReadOnly, Category="Photo")
	TObjectPtr<APlayerState> Thief = nullptr;

	UPROPERTY(BlueprintReadOnly, Category="Photo")
	TObjectPtr<AActor> Relic = nullptr;

	/** 유물 보유 여부와 무관하게 상대 플레이어가 사진에 포착되었는지 나타냅니다. */
	UPROPERTY(BlueprintReadOnly, Category="Photo")
	bool bPlayerCaptured = false;

	/** 독립 플레이어 촬영 판정에서 가시율이 가장 높았던 플레이어입니다. */
	UPROPERTY(BlueprintReadOnly, Category="Photo")
	TObjectPtr<APlayerState> CapturedPlayer = nullptr;

	UPROPERTY(BlueprintReadOnly, Category="Photo")
	float CapturedPlayerVisibility = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Photo")
	float ThiefVisibility = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Photo")
	float RelicVisibility = 0.0f;

	/** 유물 증거와 독립적으로 사진 반응 대상이 촬영되었는지 나타냅니다. */
	UPROPERTY(BlueprintReadOnly, Category="Photo")
	bool bReactiveTargetSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category="Photo")
	TObjectPtr<AActor> ReactiveTarget = nullptr;

	UPROPERTY(BlueprintReadOnly, Category="Photo")
	float ReactiveTargetVisibility = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Photo")
	float ServerCaptureTime = 0.0f;
};

/** 모든 클라이언트에 공개할 최소 증거 데이터입니다. */
USTRUCT(BlueprintType)
struct NOPHOTOS_API FNPReplicatedPhotoEvidence
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Photo")
	FGuid PhotoId;

	UPROPERTY(BlueprintReadOnly, Category="Photo")
	int32 CaptureSequence = 0;

	UPROPERTY(BlueprintReadOnly, Category="Photo")
	TObjectPtr<APlayerState> Photographer = nullptr;

	UPROPERTY(BlueprintReadOnly, Category="Photo")
	TObjectPtr<APlayerState> Thief = nullptr;

	UPROPERTY(BlueprintReadOnly, Category="Photo")
	TObjectPtr<AActor> Relic = nullptr;

	UPROPERTY(BlueprintReadOnly, Category="Photo")
	int32 AwardedScore = 0;

	UPROPERTY(BlueprintReadOnly, Category="Photo")
	float ServerCaptureTime = 0.0f;
};

/** 정산 화면에서 플레이어가 공개하기로 확정한 사진 목록입니다. */
USTRUCT(BlueprintType)
struct NOPHOTOS_API FNPPlayerSelectedPhotos
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Photo")
	TObjectPtr<APlayerState> PlayerState = nullptr;

	UPROPERTY(BlueprintReadOnly, Category="Photo")
	TArray<FGuid> PhotoIds;
};
