#include "UI/Result/Result/NPResultPictureButton.h"

#include "Components/Button.h"
#include "UI/Result/Pictures/NPResultPicturePreviewPopup.h"

void UNPResultPictureButton::NativeConstruct()
{
	Super::NativeConstruct();

	if (IsValid(ShowImageButton))
	{
		ShowImageButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleShowImageButtonClicked);
		ShowImageButton->SetIsEnabled(PhotoId.IsValid());
	}

}

void UNPResultPictureButton::InitializePhoto(
	const FGuid InPhotoId,
	const FString& InCapturedPlayerName)
{
	PhotoId = InPhotoId;
	CapturedPlayerName = InCapturedPlayerName;
	if (IsValid(ShowImageButton))
	{
		ShowImageButton->SetIsEnabled(PhotoId.IsValid());
	}
}

void UNPResultPictureButton::HandleShowImageButtonClicked()
{
	OpenPreview();
}

void UNPResultPictureButton::OpenPreview() const
{
	if (!PhotoId.IsValid() || !IsValid(PreviewPopupWidgetClass))
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
	PreviewPopup->OpenForPhoto(PhotoId, CapturedPlayerName);
}

