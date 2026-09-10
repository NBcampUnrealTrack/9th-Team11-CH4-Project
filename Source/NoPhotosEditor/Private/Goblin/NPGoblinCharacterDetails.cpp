#include "Goblin/NPGoblinCharacterDetails.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Gameplay/Goblin/NPGoblinCharacter.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "NPGoblinCharacterDetails"

TSharedRef<IDetailCustomization> FNPGoblinCharacterDetails::MakeInstance()
{
	return MakeShared<FNPGoblinCharacterDetails>();
}

void FNPGoblinCharacterDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> CustomizedObjects;
	DetailBuilder.GetObjectsBeingCustomized(CustomizedObjects);
	for (const TWeakObjectPtr<UObject>& Object : CustomizedObjects)
	{
		if (ANPGoblinCharacter* Goblin = Cast<ANPGoblinCharacter>(Object.Get()))
		{
			CustomizedGoblins.Add(Goblin);
		}
	}

	IDetailCategoryBuilder& RewardCategory = DetailBuilder.EditCategory(TEXT("Goblin|Photo|Reward"));
	RewardCategory.AddCustomRow(LOCTEXT("ImportSelectedRelicsSearch", "선택한 유물 클래스 가져오기"))
	.WholeRowContent()
	[
		SNew(SButton)
		.Text(LOCTEXT("ImportSelectedRelicsButton", "선택한 유물 클래스 가져오기"))
		.ToolTipText(LOCTEXT(
			"ImportSelectedRelicsTooltip",
			"Content Browser에서 선택한 유물 Blueprint 클래스를 Photographed Relic Classes에 중복 없이 추가합니다."))
		.HAlign(HAlign_Center)
		.OnClicked(this, &FNPGoblinCharacterDetails::ImportSelectedRelicClasses)
	];
}

FReply FNPGoblinCharacterDetails::ImportSelectedRelicClasses()
{
	for (const TWeakObjectPtr<ANPGoblinCharacter>& Goblin : CustomizedGoblins)
	{
		if (Goblin.IsValid())
		{
			Goblin->ImportSelectedPhotographedRelicClasses();
		}
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
