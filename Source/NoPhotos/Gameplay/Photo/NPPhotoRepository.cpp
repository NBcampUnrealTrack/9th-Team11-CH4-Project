#include "Gameplay/Photo/NPPhotoRepository.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Gameplay/Photo/NPPhotoLog.h"
#include "Core/Main/NPMainGameMode.h"

void UNPPhotoRepository::Initialize(ANPMainGameMode* InGameMode)
{
	OwningGameMode = InGameMode;
}

void UNPPhotoRepository::AuthorizeCapture(
	APlayerController* Photographer,
	const FGuid& PhotoId,
	const uint16 CaptureSequence)
{
	if (!IsValid(Photographer) || !PhotoId.IsValid())
	{
		return;
	}

	AuthorizedCaptures.RemoveAll(
		[Photographer, PhotoId](const FAuthorizedCapture& Entry)
		{
			return !Entry.Photographer.IsValid()
				|| (Entry.Photographer.Get() == Photographer
					&& Entry.PhotoId == PhotoId);
		});
	AuthorizedCaptures.Add({Photographer, PhotoId, CaptureSequence});
}

bool UNPPhotoRepository::IsCaptureAuthorized(
	APlayerController* Photographer,
	const FGuid& PhotoId,
	const uint16 CaptureSequence) const
{
	return AuthorizedCaptures.ContainsByPredicate(
		[Photographer, PhotoId, CaptureSequence](const FAuthorizedCapture& Entry)
		{
			return Entry.Photographer.Get() == Photographer
				&& Entry.PhotoId == PhotoId
				&& Entry.CaptureSequence == CaptureSequence;
		});
}

bool UNPPhotoRepository::StorePhoto(
	APlayerController* Photographer,
	const FGuid& PhotoId,
	const uint16 CaptureSequence,
	const int32 Width,
	const int32 Height,
	TArray<uint8>&& JpegData)
{
	if (!PhotoId.IsValid()
		|| !IsCaptureAuthorized(Photographer, PhotoId, CaptureSequence)
		|| JpegData.IsEmpty()
		|| StoredPhotos.Contains(PhotoId))
	{
		return false;
	}

	FNPStoredPhoto StoredPhoto;
	StoredPhoto.PhotoId = PhotoId;
	StoredPhoto.CaptureSequence = CaptureSequence;
	StoredPhoto.Photographer = Photographer ? Photographer->PlayerState : nullptr;
	StoredPhoto.Width = Width;
	StoredPhoto.Height = Height;
	StoredPhoto.JpegData = MoveTemp(JpegData);
	StoredByteCount += StoredPhoto.JpegData.Num();
	StoredPhotos.Add(PhotoId, MoveTemp(StoredPhoto));
	StorageOrder.Add(PhotoId);
	AuthorizedCaptures.RemoveAll(
		[Photographer, PhotoId](const FAuthorizedCapture& Entry)
		{
			return Entry.Photographer.Get() == Photographer
				&& Entry.PhotoId == PhotoId;
		});
	TrimOldestPhotos();

	if (OwningGameMode.IsValid())
	{
		OwningGameMode->HandlePhotoStored(Photographer, PhotoId);
	}
	UE_LOG(LogNPPhoto, Log, TEXT("[PhotoRepository] Stored PhotoId=%s Sequence=%u"), *PhotoId.ToString(), CaptureSequence);
	return true;
}

const FNPStoredPhoto* UNPPhotoRepository::FindPhoto(const FGuid& PhotoId) const
{
	return StoredPhotos.Find(PhotoId);
}

void UNPPhotoRepository::TrimOldestPhotos()
{
	while ((StoredPhotos.Num() > MaximumStoredPhotos || StoredByteCount > MaximumStoredBytes)
		&& !StorageOrder.IsEmpty())
	{
		const FGuid OldestId = StorageOrder[0];
		StorageOrder.RemoveAt(0);
		if (const FNPStoredPhoto* RemovedPhoto = StoredPhotos.Find(OldestId))
		{
			StoredByteCount -= RemovedPhoto->JpegData.Num();
		}
		StoredPhotos.Remove(OldestId);
	}
}
