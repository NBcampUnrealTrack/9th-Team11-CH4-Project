#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "NPPhotoTransferComponent.generated.h"

class UTexture2D;
class UNPPhotoImageCodec;
class ANPMainGameState;

USTRUCT()
struct FNPPhotoTransferHeader
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid PhotoId;

	UPROPERTY()
	uint16 CaptureSequence = 0;

	UPROPERTY()
	int32 TotalBytes = 0;

	UPROPERTY()
	int32 TotalChunks = 0;

	UPROPERTY()
	int32 Width = 0;

	UPROPERTY()
	int32 Height = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FNPOnPhotoTextureReceived,
	FGuid,
	PhotoId,
	UTexture2D*,
	Texture);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FNPOnPhotoDownloadFailed,
	FGuid,
	PhotoId);

/** JPEG의 분할 업로드, 서버 조립, 대상 클라이언트 다운로드를 담당합니다. */
UCLASS(ClassGroup=(Photo), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPPhotoTransferComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNPPhotoTransferComponent();

	bool BeginUploadPhoto(
		const FGuid& PhotoId,
		uint16 CaptureSequence,
		const TArray<uint8>& JpegData,
		int32 Width,
		int32 Height);

	UFUNCTION(BlueprintCallable, Category="Photo|Transfer")
	void RequestPhoto(const FGuid& PhotoId, bool bPrioritize = false);

	UFUNCTION(BlueprintPure, Category="Photo|Transfer")
	UTexture2D* FindReceivedPhoto(const FGuid& PhotoId) const;
	bool SaveReceivedPhotoToDisk(const FGuid& PhotoId, FString& OutSavedPath) const;

	UPROPERTY(BlueprintAssignable, Category="Photo|Transfer")
	FNPOnPhotoTextureReceived OnPhotoTextureReceived;

	UPROPERTY(BlueprintAssignable, Category="Photo|Transfer")
	FNPOnPhotoDownloadFailed OnPhotoDownloadFailed;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	struct FOutgoingTransfer
	{
		FNPPhotoTransferHeader Header;
		TArray<uint8> Data;
		int32 NextChunkIndex = 0;
		bool bFinishSent = false;
	};

	struct FIncomingTransfer
	{
		FNPPhotoTransferHeader Header;
		TArray<uint8> Data;
		int32 NextChunkIndex = 0;
	};

	static bool IsValidHeader(const FNPPhotoTransferHeader& Header);
	static void BuildChunk(
		const FOutgoingTransfer& Transfer,
		int32 ChunkIndex,
		TArray<uint8>& OutChunk);
	void StartNextUpload();
	void EnsurePhotoEvidenceBinding();
	void StartNextDownloadRequest();
	void SendActiveDownloadRequest();
	void RestartDownloadTimeout();
	void HandleDownloadAttemptFailed();
	void HandleDownloadRequestTimeout();

	UFUNCTION()
	void HandlePhotoEvidenceChanged();

	UFUNCTION(Server, Reliable)
	void ServerBeginPhotoUpload(const FNPPhotoTransferHeader& Header);

	UFUNCTION(Server, Reliable)
	void ServerUploadPhotoChunk(FGuid PhotoId, int32 ChunkIndex, const TArray<uint8>& ChunkData);

	UFUNCTION(Server, Reliable)
	void ServerFinishPhotoUpload(FGuid PhotoId);

	UFUNCTION(Server, Reliable)
	void ServerRequestPhoto(FGuid PhotoId);

	UFUNCTION(Client, Reliable)
	void ClientBeginPhotoDownload(const FNPPhotoTransferHeader& Header);

	UFUNCTION(Client, Reliable)
	void ClientReceivePhotoChunk(FGuid PhotoId, int32 ChunkIndex, const TArray<uint8>& ChunkData);

	UFUNCTION(Client, Reliable)
	void ClientFinishPhotoDownload(FGuid PhotoId);

	TOptional<FOutgoingTransfer> PendingUpload;
	TArray<FOutgoingTransfer> QueuedUploads;
	TOptional<FIncomingTransfer> IncomingUpload;
	TOptional<FOutgoingTransfer> PendingDownload;
	TOptional<FIncomingTransfer> IncomingDownload;
	TArray<FGuid> QueuedDownloadPhotoIds;
	TSet<FGuid> KnownDownloadPhotoIds;
	FGuid ActiveDownloadPhotoId;
	int32 ActiveDownloadRetryCount = 0;
	FTimerHandle DownloadTimeoutTimer;

	UPROPERTY(Transient)
	TObjectPtr<UNPPhotoImageCodec> ImageCodec;

	UPROPERTY(Transient)
	TMap<FGuid, TObjectPtr<UTexture2D>> ReceivedPhotoTextures;
	TMap<FGuid, TArray<uint8>> ReceivedPhotoJpegData;

	UPROPERTY(Transient)
	TObjectPtr<ANPMainGameState> ObservedGameState;

	static constexpr int32 ChunkSize = 16 * 1024;
	static constexpr int32 MaximumPhotoBytes = 256 * 1024;
	static constexpr int32 MaximumDimension = 2048;
	static constexpr int32 MaximumDownloadRetryCount = 1;
	static constexpr float DownloadRequestTimeoutSeconds = 5.0f;
};
