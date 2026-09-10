#include "Santa/NPSantaEventDefinitionDetails.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Gameplay/MapEvents/Santa/NPSantaEventDefinition.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "NPSantaEventDefinitionDetails"

TSharedRef<IDetailCustomization> FNPSantaEventDefinitionDetails::MakeInstance()
{
	return MakeShared<FNPSantaEventDefinitionDetails>();
}

void FNPSantaEventDefinitionDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> CustomizedObjects;
	DetailBuilder.GetObjectsBeingCustomized(CustomizedObjects);
	for (const TWeakObjectPtr<UObject>& Object : CustomizedObjects)
	{
		if (UNPSantaEventDefinition* Definition = Cast<UNPSantaEventDefinition>(Object.Get()))
		{
			CustomizedDefinitions.Add(Definition);
		}
	}

	IDetailCategoryBuilder& GiftsCategory = DetailBuilder.EditCategory(TEXT("Santa Event|Gifts"));
	GiftsCategory.AddCustomRow(LOCTEXT("ImportSelectedRelicsSearch", "선택한 유물 클래스 가져오기"))
	.WholeRowContent()
	[
		SNew(SButton)
		.Text(LOCTEXT("ImportSelectedRelicsButton", "선택한 유물 클래스 가져오기"))
		.ToolTipText(LOCTEXT(
			"ImportSelectedRelicsTooltip",
			"Content Browser에서 선택한 유물 Blueprint 클래스를 Relic Classes에 중복 없이 추가합니다."))
		.HAlign(HAlign_Center)
		.OnClicked(this, &FNPSantaEventDefinitionDetails::ImportSelectedRelicClasses)
	];
}

FReply FNPSantaEventDefinitionDetails::ImportSelectedRelicClasses()
{
	for (const TWeakObjectPtr<UNPSantaEventDefinition>& Definition : CustomizedDefinitions)
	{
		if (Definition.IsValid())
		{
			Definition->ImportSelectedRelicClasses();
		}
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
