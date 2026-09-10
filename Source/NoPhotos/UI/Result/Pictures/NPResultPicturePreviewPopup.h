#pragma once

#include "CoreMinimal.h"
#include "UI/NPUserWidget.h"
#include "NPResultPicturePreviewPopup.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UTexture2D;
class ANPMainGameState;
class UNPPhotoTransferComponent;

UCLASS()
class NOPHOTOS_API UNPResultPicturePreviewPopup : public UNPUserWidget
{
	GENERATED_BODY()

public:
	void OpenForPhoto(FGuid InPhotoId, const FString& InCapturedPlayerName = FString());

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UFUNCTION()
	void HandleCloseClicked();
	UFUNCTION()
	void HandleDownloadClicked();
	UFUNCTION()
	void HandleLikeClicked();
	UFUNCTION()
	void HandlePhotoLikesChanged(FGuid ChangedPhotoId);
	UFUNCTION()
	void HandlePhotoFullyLiked(FGuid FullyLikedPhotoId, int32 LikeCount);
	UFUNCTION()
	void HandlePhotoTextureReceived(FGuid ReceivedPhotoId, UTexture2D* Texture);
	void HandleDownloadTimeout();

	void EnsureDownloadButton();
	void EnsureLikeControls();
	void DisplayPhoto(UTexture2D* Texture);
	void SetDownloadButtonText(const FText& Text) const;
	void RefreshLikeState();
	void StartFullyLikedPulse();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> PreviewImage;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> DownloadButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DownloadButtonText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CapturedPlayerText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> LikeButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LikeButtonText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LikeCountText;

	UPROPERTY(EditDefaultsOnly, Category = "UI|Like", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float FullyLikedPulseScale = 1.3f;

	UPROPERTY(EditDefaultsOnly, Category = "UI|Like", meta = (ClampMin = "0.01", UIMin = "0.01", Units = "s"))
	float FullyLikedPulseDuration = 0.4f;

	UPROPERTY(Transient)
	TObjectPtr<UNPPhotoTransferComponent> TransferComponent;

	UPROPERTY(Transient)
	TObjectPtr<ANPMainGameState> ObservedGameState;

	FGuid PhotoId;
	bool bPhotoRequestPending = false;
	bool bLikeRequestPending = false;
	bool bFullyLikedPulseActive = false;
	float FullyLikedPulseElapsed = 0.0f;
	FVector2D FullyLikedPulseBaseScale = FVector2D(1.0f);
	FTimerHandle DownloadTimeoutTimer;
	static constexpr float DownloadTimeoutSeconds = 5.0f;
};
