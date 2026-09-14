#include "UI/Result/Pictures/NPShowPicture.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

void UNPShowPicture::NativeConstruct()
{
	Super::NativeConstruct();

	if (IsValid(SelectButton))
	{
		SelectButton->OnClicked.AddDynamic(
			this,
			&UNPShowPicture::OnSelectButtonClicked);
	}

	SetSelected(false);
}

void UNPShowPicture::SetPicture(
	UTexture2D* InTexture,
	const int32 InPictureIndex)
{
	if (!IsValid(SelectPicture) || !IsValid(InTexture))
	{
		return;
	}

	CurrentPictureIndex = InPictureIndex;
	SelectPicture->SetBrushFromTexture(InTexture);
}

void UNPShowPicture::SetSelected(const bool InSelected)
{
	IsSelected = InSelected;

	if (!IsValid(StampIcon))
	{
		return;
	}

	UTexture2D* StampTexture = IsSelected
		? SelectedStampTexture.Get()
		: UnselectedStampTexture.Get();
	if (IsValid(StampTexture))
	{
		StampIcon->SetBrushFromTexture(StampTexture);
	}
}

void UNPShowPicture::SetSelectionEnabled(const bool bEnabled)
{
	if (IsValid(SelectButton))
	{
		SelectButton->SetIsEnabled(bEnabled);
	}
}

void UNPShowPicture::OnSelectButtonClicked()
{
	if (CurrentPictureIndex == INDEX_NONE)
	{
		return;
	}

	OnSelectRequested.Broadcast(CurrentPictureIndex);
}
