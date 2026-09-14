#include "UI/Result/Pictures/NPSelectedPictureListWidget.h"

#include "Components/HorizontalBox.h"
#include "Engine/Texture2D.h"
#include "UI/Result/Pictures/NPSelectedPictureWidget.h"

void UNPSelectedPictureListWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ClearSelectedPictures();
}

void UNPSelectedPictureListWidget::AddSelectedPicture(
	UTexture2D* InTexture,
	const int32 PictureIndex)
{
	if (!IsValid(SelectedPictureList) || !IsValid(SelectedPictureWidgetClass)
		|| !IsValid(InTexture) || PictureIndices.Contains(PictureIndex))
	{
		return;
	}

	UNPSelectedPictureWidget* SelectedPictureWidget =
		CreateWidget<UNPSelectedPictureWidget>(GetOwningPlayer(), SelectedPictureWidgetClass);

	if (!IsValid(SelectedPictureWidget))
	{
		return;
	}

	SelectedPictureWidget->SetPicture(InTexture);
	SelectedPictureWidget->SetRemovalEnabled(bRemovalEnabled);
	SelectedPictureWidget->OnPictureClicked.AddUniqueDynamic(
		this,
		&ThisClass::HandlePictureWidgetClicked);
	SelectedPictureWidget->OnPictureRemoveRequested.AddUniqueDynamic(
		this,
		&ThisClass::HandlePictureWidgetRemoveRequested);

	SelectedPictureList->AddChild(SelectedPictureWidget);
	SelectedPictureWidgets.Add(SelectedPictureWidget);
	PictureIndices.Add(PictureIndex);
}

void UNPSelectedPictureListWidget::RemoveSelectedPicture(const int32 PictureIndex)
{
	const int32 WidgetIndex = PictureIndices.IndexOfByKey(PictureIndex);
	if (WidgetIndex == INDEX_NONE)
	{
		return;
	}

	if (SelectedPictureWidgets.IsValidIndex(WidgetIndex)
		&& IsValid(SelectedPictureWidgets[WidgetIndex]))
	{
		SelectedPictureWidgets[WidgetIndex]->RemoveFromParent();
	}

	SelectedPictureWidgets.RemoveAt(WidgetIndex);
	PictureIndices.RemoveAt(WidgetIndex);
}

void UNPSelectedPictureListWidget::ClearSelectedPictures()
{
	if (IsValid(SelectedPictureList))
	{
		SelectedPictureList->ClearChildren();
	}

	SelectedPictureWidgets.Empty();
	PictureIndices.Empty();
}

void UNPSelectedPictureListWidget::SetRemovalEnabled(const bool bEnabled)
{
	bRemovalEnabled = bEnabled;

	for (UNPSelectedPictureWidget* SelectedPictureWidget : SelectedPictureWidgets)
	{
		if (IsValid(SelectedPictureWidget))
		{
			SelectedPictureWidget->SetRemovalEnabled(bRemovalEnabled);
		}
	}
}

void UNPSelectedPictureListWidget::HandlePictureWidgetClicked(UNPSelectedPictureWidget* PictureWidget)
{
	const int32 WidgetIndex = SelectedPictureWidgets.IndexOfByKey(PictureWidget);
	if (PictureIndices.IsValidIndex(WidgetIndex))
	{
		OnPictureClicked.Broadcast(PictureIndices[WidgetIndex]);
	}
}

void UNPSelectedPictureListWidget::HandlePictureWidgetRemoveRequested(UNPSelectedPictureWidget* PictureWidget)
{
	const int32 WidgetIndex = SelectedPictureWidgets.IndexOfByKey(PictureWidget);
	if (PictureIndices.IsValidIndex(WidgetIndex))
	{
		OnPictureRemoveRequested.Broadcast(PictureIndices[WidgetIndex]);
	}
}
