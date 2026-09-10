#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "NPPhotoRepository.generated.h"

class ANPMainGameMode;
class APlayerController;
class APlayerState;

struct FNPStoredPhoto
{
	FGuid PhotoId;
	uint16 CaptureSequence = 0;
	TWeakObjectPtr<APlayerState> Photographer;
	int32 Width = 0;
	int32 Height = 0;
	TArray<uint8> JpegData;
};

/** 게임 종료 후 플레이어가 선택해 업로드한 JPEG만 제한된 메모리 안에 보관합니다. */
UCLASS()
class NOPHOTOS_API UNPPhotoRepository : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(ANPMainGameMode* InGameMode);
	void AuthorizeCapture(APlayerController* Photographer, const FGuid& PhotoId, uint16 CaptureSequence);
	bool IsCaptureAuthorized(APlayerController* Photographer, const FGuid& PhotoId, uint16 CaptureSequence) const;
	bool StorePhoto(
		APlayerController* Photographer,
		const FGuid& PhotoId,
		uint16 CaptureSequence,
		int32 Width,
		int32 Height,
		TArray<uint8>&& JpegData);
	const FNPStoredPhoto* FindPhoto(const FGuid& PhotoId) const;

private:
	struct FAuthorizedCapture
	{
		TWeakObjectPtr<APlayerController> Photographer;
		FGuid PhotoId;
		uint16 CaptureSequence = 0;
	};

	void TrimOldestPhotos();

	TWeakObjectPtr<ANPMainGameMode> OwningGameMode;
	TArray<FAuthorizedCapture> AuthorizedCaptures;
	TMap<FGuid, FNPStoredPhoto> StoredPhotos;
	TArray<FGuid> StorageOrder;
	int64 StoredByteCount = 0;

	static constexpr int32 MaximumStoredPhotos = 30;
	static constexpr int64 MaximumStoredBytes = 16 * 1024 * 1024;
};
