#pragma once

#include "CoreMinimal.h"
#include "UI/NPUserWidget.h"
#include "NPResultPictureButton.generated.h"

class UButton;
class UNPResultPicturePreviewPopup;

UCLASS()
class NOPHOTOS_API UNPResultPictureButton : public UNPUserWidget
{
	GENERATED_BODY()

public:
	void InitializePhoto(FGuid InPhotoId, const FString& InCapturedPlayerName = FString());

protected:
	virtual void NativeConstruct() override;

private:
	UFUNCTION()
	void HandleShowImageButtonClicked();

	void OpenPreview() const;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> ShowImageButton;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UNPResultPicturePreviewPopup> PreviewPopupWidgetClass;

	FGuid PhotoId;
	FString CapturedPlayerName;
};
