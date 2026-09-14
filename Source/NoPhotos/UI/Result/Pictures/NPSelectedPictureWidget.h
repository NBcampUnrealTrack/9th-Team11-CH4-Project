//화면 왼쪽 아래에 뜨는 작은 이미지

#pragma once

#include "CoreMinimal.h"
#include "UI/NPUserWidget.h"
#include "NPSelectedPictureWidget.generated.h"

class UButton;
class UImage;
class UTexture2D;
class UNPSelectedPictureWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNPSelectedPictureClicked, UNPSelectedPictureWidget*, PictureWidget);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNPSelectedPictureRemoveRequested, UNPSelectedPictureWidget*, PictureWidget);

UCLASS()
class NOPHOTOS_API UNPSelectedPictureWidget : public UNPUserWidget
{
	GENERATED_BODY()

public:
	void SetPicture(UTexture2D* InTexture);
	void SetRemovalEnabled(bool bEnabled);

	UPROPERTY(BlueprintAssignable, Category="Picture")
	FNPSelectedPictureClicked OnPictureClicked;

	UPROPERTY(BlueprintAssignable, Category="Picture")
	FNPSelectedPictureRemoveRequested OnPictureRemoveRequested;

protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	UFUNCTION()
	void HandleImageButtonClicked();

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> ImageButton;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> SelectedImage;

	bool bRemovalEnabled = true;
};
