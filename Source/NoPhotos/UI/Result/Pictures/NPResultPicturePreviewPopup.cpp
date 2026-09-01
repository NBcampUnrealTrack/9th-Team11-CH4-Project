#include "UI/Result/Pictures/NPResultPicturePreviewPopup.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

void UNPResultPicturePreviewPopup::NativeConstruct()
{
	Super::NativeConstruct();

	if (IsValid(CloseButton))
	{
		CloseButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCloseClicked);
	}
}

void UNPResultPicturePreviewPopup::OpenWithTexture(
	UTexture2D* InTexture,
	const FString& InCapturedPlayerName)
{
	if (IsValid(PreviewImage) && IsValid(InTexture))
	{
		PreviewImage->SetBrushFromTexture(InTexture);
	}

	if (IsValid(CapturedPlayerText))
	{
		const FString DisplayName = InCapturedPlayerName.IsEmpty() ? TEXT("익명") : InCapturedPlayerName;
		CapturedPlayerText->SetText(FText::FromString(FString::Printf(TEXT("%s 님이 찍힌 사진"), *DisplayName)));
	}
}

void UNPResultPicturePreviewPopup::HandleCloseClicked()
{
	RemoveFromParent();
}
