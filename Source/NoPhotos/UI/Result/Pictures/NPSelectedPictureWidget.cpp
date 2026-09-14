#include "UI/Result/Pictures/NPSelectedPictureWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Input/Events.h"

void UNPSelectedPictureWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (IsValid(ImageButton))
	{
		ImageButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleImageButtonClicked);
	}
}

FReply UNPSelectedPictureWidget::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		OnPictureClicked.Broadcast(this);
		return FReply::Handled();
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		if (bRemovalEnabled)
		{
			OnPictureRemoveRequested.Broadcast(this);
		}
		return FReply::Handled();
	}

	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

void UNPSelectedPictureWidget::SetPicture(UTexture2D* InTexture)
{
	if (IsValid(SelectedImage) && IsValid(InTexture))
	{
		SelectedImage->SetBrushFromTexture(InTexture);
	}
}

void UNPSelectedPictureWidget::SetRemovalEnabled(const bool bEnabled)
{
	bRemovalEnabled = bEnabled;
}

void UNPSelectedPictureWidget::HandleImageButtonClicked()
{
	OnPictureClicked.Broadcast(this);
}
