#pragma once

#include "CoreMinimal.h"
#include "UI/NPUserWidget.h"
#include "NPResultPicturePreviewPopup.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UTexture2D;

UCLASS()
class NOPHOTOS_API UNPResultPicturePreviewPopup : public UNPUserWidget
{
	GENERATED_BODY()

public:
	void OpenWithTexture(UTexture2D* InTexture, const FString& InCapturedPlayerName = FString());

protected:
	virtual void NativeConstruct() override;

private:
	UFUNCTION()
	void HandleCloseClicked();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> PreviewImage;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CapturedPlayerText;
};
