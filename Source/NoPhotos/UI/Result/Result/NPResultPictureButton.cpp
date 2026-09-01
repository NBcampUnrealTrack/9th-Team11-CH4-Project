#include "UI/Result/Result/NPResultPictureButton.h"

#include "Components/Button.h"
#include "Engine/Texture2D.h"
#include "UI/Result/Pictures/NPResultPicturePreviewPopup.h"

void UNPResultPictureButton::NativeConstruct()
{
	Super::NativeConstruct();

	if (IsValid(ShowImageButton))
	{
		ShowImageButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleShowImageButtonClicked);
		ShowImageButton->SetIsEnabled(IsValid(PictureTexture));
	}

}

void UNPResultPictureButton::SetPictureTexture(
	UTexture2D* InTexture,
	const FString& InCapturedPlayerName)
{
	if (!IsValid(InTexture))
	{
		return;
	}

	PictureTexture = InTexture;
	CapturedPlayerName = InCapturedPlayerName;
	if (IsValid(ShowImageButton))
	{
		ShowImageButton->SetIsEnabled(true);
	}
}

void UNPResultPictureButton::HandleShowImageButtonClicked()
{
	OpenPreview();
}

void UNPResultPictureButton::OpenPreview() const
{
	if (!IsValid(PictureTexture) || !IsValid(PreviewPopupWidgetClass))
	{
		return;
	}

	UNPResultPicturePreviewPopup* PreviewPopup =
		CreateWidget<UNPResultPicturePreviewPopup>(GetOwningPlayer(), PreviewPopupWidgetClass);
	if (!IsValid(PreviewPopup))
	{
		return;
	}

	PreviewPopup->AddToViewport(200);
	PreviewPopup->OpenWithTexture(PictureTexture, CapturedPlayerName);
}

