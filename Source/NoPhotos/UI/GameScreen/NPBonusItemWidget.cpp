#include "UI/GameScreen/NPBonusItemWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Gameplay/Relic/NPBaseRelic.h"
#include "UI/GameScreen/Relic/NPShowRelicLocationComponent.h"

UNPBonusItemWidget::UNPBonusItemWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UNPBonusItemWidget::SetItemName(const FString& InItemName)
{
	if (IsValid(ItemNameText))
	{
		ItemNameText->SetText(FText::FromString(InItemName));
	}
}

void UNPBonusItemWidget::SetItemColor(const FLinearColor& InItemColor)
{
	ActiveItemColor = InItemColor;
	if (IsValid(ItemNameText))
	{
		ItemNameText->SetColorAndOpacity(FSlateColor(
			bIsReturned ? FLinearColor::Gray : ActiveItemColor));
	}
}

void UNPBonusItemWidget::SetRelic(ANPBaseRelic* InRelic)
{
	if (BoundRelicLocationComponent.IsValid())
	{
		BoundRelicLocationComponent->OnRelicReturned.RemoveDynamic(
			this,
			&ThisClass::HandleRelicReturned);
	}

	AssignedRelic = InRelic;
	BoundRelicLocationComponent = IsValid(InRelic)
		? InRelic->FindComponentByClass<UNPShowRelicLocationComponent>()
		: nullptr;
	if (BoundRelicLocationComponent.IsValid())
	{
		BoundRelicLocationComponent->OnRelicReturned.AddUniqueDynamic(
			this,
			&ThisClass::HandleRelicReturned);
	}

	RefreshReturnedState();
}

void UNPBonusItemWidget::RefreshReturnedState()
{
	UpdateReturnedState();
}

void UNPBonusItemWidget::NativeDestruct()
{
	if (BoundRelicLocationComponent.IsValid())
	{
		BoundRelicLocationComponent->OnRelicReturned.RemoveDynamic(
			this,
			&ThisClass::HandleRelicReturned);
	}

	BoundRelicLocationComponent.Reset();
	Super::NativeDestruct();
}

void UNPBonusItemWidget::HandleRelicReturned()
{
	SetReturnedState(true);
}

void UNPBonusItemWidget::UpdateReturnedState()
{
	SetReturnedState(
		AssignedRelic.IsValid()
		&& AssignedRelic->IsBonusQuestResolved());
}

void UNPBonusItemWidget::SetReturnedState(const bool bInIsReturned)
{
	if (bHasReturnedState && bIsReturned == bInIsReturned)
	{
		return;
	}

	bHasReturnedState = true;
	bIsReturned = bInIsReturned;
	if (IsValid(ItemNameText))
	{
		ItemNameText->SetColorAndOpacity(FSlateColor(
			bIsReturned ? FLinearColor::Gray : ActiveItemColor));
	}

	if (IsValid(StrikeThroughLine))
	{
		StrikeThroughLine->SetVisibility(
			bIsReturned ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}
