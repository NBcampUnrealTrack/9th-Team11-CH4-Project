#pragma once

#include "CoreMinimal.h"
#include "UI/NPUserWidget.h"
#include "NPResultPicturePreviewPopup.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UTexture2D;
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

private:
	UFUNCTION()
	void HandleCloseClicked();
	UFUNCTION()
	void HandleDownloadClicked();
	UFUNCTION()
	void HandlePhotoTextureReceived(FGuid ReceivedPhotoId, UTexture2D* Texture);
	void HandleDownloadTimeout();

	void EnsureDownloadButton();
	void DisplayPhoto(UTexture2D* Texture);
	void SetDownloadButtonText(const FText& Text) const;

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

	UPROPERTY(Transient)
	TObjectPtr<UNPPhotoTransferComponent> TransferComponent;

	FGuid PhotoId;
	bool bPhotoRequestPending = false;
	FTimerHandle DownloadTimeoutTimer;
	static constexpr float DownloadTimeoutSeconds = 5.0f;
};
