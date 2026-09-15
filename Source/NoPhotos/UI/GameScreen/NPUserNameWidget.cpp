#include "UI/GameScreen/NPUserNameWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerState.h"

void UNPUserNameWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetPhotoTargetIndicatorVisible(false);
	RefreshPlayerName();
}

void UNPUserNameWidget::SetTargetPlayerState(APlayerState* InPlayerState)
{
	TargetPlayerState = InPlayerState;
	RefreshPlayerName();
}

void UNPUserNameWidget::SetPhotoTargetIndicatorVisible(const bool bVisible)
{
	if (IsValid(PhotoTargetIcon))
	{
		PhotoTargetIcon->SetVisibility(
			bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UNPUserNameWidget::RefreshPlayerName()
{
	if (!IsValid(UserNameText))
	{
		return;
	}

	if (!IsValid(TargetPlayerState))
	{
		UserNameText->SetText(FText::GetEmpty());
		return;
	}

	UserNameText->SetText(FText::FromString(TargetPlayerState->GetPlayerName()));
}
