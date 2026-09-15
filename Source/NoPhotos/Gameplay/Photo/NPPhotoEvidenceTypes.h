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

/** 한 장의 사진에서 동일한 유물을 들고 있다가 적발된 플레이어 묶음입니다. */
USTRUCT(BlueprintType)
struct NOPHOTOS_API FNPPhotoRelicEvidenceGroup
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Photo")
	TObjectPtr<AActor> Relic = nullptr;

	UPROPERTY(BlueprintReadOnly, Category="Photo")
	TArray<TObjectPtr<APlayerState>> Thieves;
};

/** 사진 한 장에 대한 서버의 상세 판정 결과입니다. */
USTRUCT(BlueprintType)
struct NOPHOTOS_API FNPPhotoEvidenceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Photo")
	FGuid PhotoId;

	UPROPERTY(BlueprintReadOnly, Category="Photo")
	int32 CaptureSequence = 0;

	UPROPERTY(BlueprintReadOnly, Category="Photo")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category="Photo")
	ENPPhotoEvidenceFailureReason FailureReason = ENPPhotoEvidenceFailureReason::None;

	UPROPERTY(BlueprintReadOnly, Category="Photo")
	TObjectPtr<APlayerState> Photographer = nullptr;

	UPROPERTY(BlueprintReadOnly, Category="Photo")
	TArray<FNPPhotoRelicEvidenceGroup> RelicEvidenceGroups;

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
	TArray<TObjectPtr<APlayerState>> Thieves;

	UPROPERTY(BlueprintReadOnly, Category="Photo")
	TArray<TObjectPtr<AActor>> Relics;

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

/** 정산 화면에서 사진 한 장이 받은 좋아요 상태입니다. */
USTRUCT(BlueprintType)
struct NOPHOTOS_API FNPPhotoLikeState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Photo|Like")
	FGuid PhotoId;

	UPROPERTY(BlueprintReadOnly, Category="Photo|Like")
	TArray<TObjectPtr<APlayerState>> LikedPlayerStates;
};
