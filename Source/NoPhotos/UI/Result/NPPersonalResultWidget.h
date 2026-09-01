#pragma once

#include "CoreMinimal.h"
#include "UI/NPUserWidget.h"
#include "NPPersonalResultWidget.generated.h"

class APlayerState;
class UHorizontalBox;
class UTextBlock;
class UTexture2D;
class UNPPhotoTransferComponent;
class UNPResultPictureButton;

UCLASS()
class NOPHOTOS_API UNPPersonalResultWidget : public UNPUserWidget
{
	GENERATED_BODY()

public:
	void SetupResult(int32 InRank, const FString& InPlayerName,	int32 InScore,	APlayerState* InPlayerState);

protected:
	virtual void NativeDestruct() override;

private:
	void CreatePictureButtons();
	void RequestNextPhoto();

	UFUNCTION()
	void HandlePhotoTextureReceived(FGuid PhotoId, UTexture2D* Texture);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> RankText;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PlayerNameText;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> PictureList;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ScoreText;
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UNPResultPictureButton> PictureButtonWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<APlayerState> ResultPlayerState;
	UPROPERTY(Transient)
	TObjectPtr<UNPPhotoTransferComponent> TransferComponent;
	UPROPERTY(Transient)
	TMap<FGuid, TObjectPtr<UNPResultPictureButton>> PictureButtonsById;

	TArray<FGuid> PendingPhotoIds;
	FGuid DownloadingPhotoId;
};
