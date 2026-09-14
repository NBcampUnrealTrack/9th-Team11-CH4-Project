#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Gameplay/Photo/NPPhotoEvidenceTypes.h"
#include "NPPhotoCaptureComponent.generated.h"

class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UTexture2D;
class ANPStablePhysicsPawn;
class UNPPhotoImageCodec;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FNPOnPhotoResultReceived,
	FNPPhotoEvidenceResult,
	Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FNPOnPhotoCaptured,
	UTextureRenderTarget2D*,
	Photo);

/** 로컬 사진 이미지를 만들고 소유 PlayerController에서 서버 판정을 요청합니다. */
UCLASS(ClassGroup=(Photo), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPPhotoCaptureComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNPPhotoCaptureComponent();

	UFUNCTION(BlueprintCallable, Category="Photo")
	bool TakePhoto();

	/** 대기방처럼 이미지 생성 없이 줌·플래시·셔터 연출만 사용할지 설정합니다. */
	void SetPresentationOnly(const bool bEnabled)
	{
		bPresentationOnly = bEnabled;
	}

	/** Ability가 쿨다운을 소비하기 전에 로컬 카메라 상태를 검사합니다. */
	bool CanTakePhotoLocally() const;

	UFUNCTION(BlueprintCallable, Category="Photo")
	void TogglePhotoMode();

	UFUNCTION(BlueprintCallable, Category="Photo")
	bool EnterPhotoMode();

	UFUNCTION(BlueprintCallable, Category="Photo")
	void ExitPhotoMode();

	UFUNCTION(BlueprintPure, Category="Photo")
	bool IsPhotoModeActive() const { return bPhotoModeActive; }

	UFUNCTION(BlueprintPure, Category="Photo")
	UTextureRenderTarget2D* GetPhotoRenderTarget() const { return PhotoRenderTarget; }

	void ResetLocalPhotos();
	UTexture2D* FindLocalPhotoTexture(const FGuid& PhotoId);
	bool GetLocalPhotoData(
		const FGuid& PhotoId,
		uint16& OutCaptureSequence,
		const TArray<uint8>*& OutJpegData,
		int32& OutWidth,
		int32& OutHeight) const;

	UPROPERTY(BlueprintAssignable, Category="Photo")
	FNPOnPhotoResultReceived OnPhotoResultReceived;

	/** 로컬 Scene Capture가 끝난 직후 촬영된 Render Target을 전달합니다. */
	UPROPERTY(BlueprintAssignable, Category="Photo")
	FNPOnPhotoCaptured OnPhotoCaptured;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Photo|Capture", meta=(ClampMin="64"))
	int32 CaptureWidth = 1024;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Photo|Capture", meta=(ClampMin="64"))
	int32 CaptureHeight = 576;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Photo|Capture", meta=(ClampMin="1.0", ClampMax="179.0"))
	float CaptureFOV = 60.0f;

	/** 촬영 직후 다음 촬영까지 기다려야 하는 시간입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Photo|Rules", meta=(ClampMin="0.0"))
	float PhotoCooldown = 5.0f;

private:
	struct FLocalCorrectPhoto
	{
		uint16 CaptureSequence = 0;
		TArray<uint8> JpegData;
		int32 Width = 0;
		int32 Height = 0;
	};

	void InitializeLocalCapture();
	void ApplySceneCaptureLightingSettings();
	bool SynchronizeSceneCapture(
		APlayerController* PlayerController,
		ANPStablePhysicsPawn* Pawn);
	void FinalizePendingCapture();
	bool IsPhotographerGrabbing() const;
	void CancelPhotoAttempt();
	void StoreLocalCorrectPhoto(
		const FGuid& PhotoId,
		uint16 CaptureSequence,
		TArray<uint8>&& JpegData);

	UFUNCTION(Server, Reliable)
	void ServerRequestTakePhoto(
		FVector_NetQuantize10 CameraLocation,
		FVector_NetQuantizeNormal CameraForward,
		uint16 CaptureSequence);

	UFUNCTION(Client, Reliable)
	void ClientReceivePhotoResult(const FNPPhotoEvidenceResult& Result);

	UPROPERTY(Transient)
	TObjectPtr<USceneCaptureComponent2D> SceneCapture = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> PhotoRenderTarget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UNPPhotoImageCodec> ImageCodec = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="Photo|Transfer", meta=(ClampMin="1", ClampMax="100"))
	int32 JpegQuality = 60;

	TMap<uint16, TArray<uint8>> PendingJpegPhotos;
	TMap<FGuid, FLocalCorrectPhoto> LocalCorrectPhotos;
	TArray<FGuid> LocalPhotoOrder;

	UPROPERTY(Transient)
	TMap<FGuid, TObjectPtr<UTexture2D>> LocalPhotoTextures;

	static constexpr int32 MaximumLocalCorrectPhotos = 30;
	static constexpr int32 CaptureWarmupFrameCount = 3;

	double LastServerCaptureTime = -TNumericLimits<double>::Max();
	FVector PendingCaptureLocation = FVector::ZeroVector;
	FRotator PendingCaptureRotation = FRotator::ZeroRotator;
	TWeakObjectPtr<ANPStablePhysicsPawn> PendingCapturePawn;
	int32 PendingCaptureFramesRemaining = 0;
	uint16 NextCaptureSequence = 0;
	bool bPhotoAttemptInProgress = false;
	bool bPhotoModeActive = false;
	bool bPresentationOnly = false;
	TWeakObjectPtr<ANPStablePhysicsPawn> PhotoModePawn;
};
