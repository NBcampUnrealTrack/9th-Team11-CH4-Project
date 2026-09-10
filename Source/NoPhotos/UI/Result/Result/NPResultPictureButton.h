#pragma once

#include "CoreMinimal.h"
#include "UI/NPUserWidget.h"
#include "NPResultPictureButton.generated.h"

class UButton;
class UTextBlock;
class ANPMainGameState;
class UNPResultPicturePreviewPopup;

UCLASS()
class NOPHOTOS_API UNPResultPictureButton : public UNPUserWidget
{
	GENERATED_BODY()

public:
	void InitializePhoto(FGuid InPhotoId, const FString& InCapturedPlayerName = FString());

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UFUNCTION()
	void HandleShowImageButtonClicked();
	UFUNCTION()
	void HandleLikeButtonClicked();
	UFUNCTION()
	void HandlePhotoLikesChanged(FGuid ChangedPhotoId);
	UFUNCTION()
	void HandlePhotoFullyLiked(FGuid FullyLikedPhotoId, int32 LikeCount);

	void OpenPreview() const;
	void EnsureLikeButton();
	void RefreshLikeState();
	void StartFullyLikedPulse();

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> ShowImageButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> LikeButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LikeCountText;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UNPResultPicturePreviewPopup> PreviewPopupWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "UI|Like", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float FullyLikedPulseScale = 1.3f;

	UPROPERTY(EditDefaultsOnly, Category = "UI|Like", meta = (ClampMin = "0.01", UIMin = "0.01", Units = "s"))
	float FullyLikedPulseDuration = 0.4f;

	UPROPERTY(Transient)
	TObjectPtr<ANPMainGameState> ObservedGameState;

	FGuid PhotoId;
	FString CapturedPlayerName;
	bool bLikeRequestPending = false;
	bool bFullyLikedPulseActive = false;
	float FullyLikedPulseElapsed = 0.0f;
	FVector2D FullyLikedPulseBaseScale = FVector2D(1.0f);
};
