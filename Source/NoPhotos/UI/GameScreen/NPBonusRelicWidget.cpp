#include "UI/GameScreen/NPBonusRelicWidget.h"
#include "UI/GameScreen/NPBonusItemWidget.h"
#include "Data/Structs/NPRelicData.h"
#include "Gameplay/Relic/Components/NPPlayerBonusQuestComponent.h"
#include "Gameplay/Relic/NPBaseRelic.h"
#include "UI/GameScreen/Relic/NPShowRelicLocationComponent.h"
#include "Components/VerticalBox.h"
#include "GameFramework/PlayerController.h"

UNPBonusRelicWidget::UNPBonusRelicWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UNPBonusRelicWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindToBonusQuestComponent();
	InitBonusItemList();
}

void UNPBonusRelicWidget::NativeDestruct()
{
	if (BoundBonusQuestComponent.IsValid())
	{
		BoundBonusQuestComponent->OnAssignedQuestRelicsChanged.RemoveDynamic(
			this,
			&ThisClass::HandleAssignedQuestRelicsChanged);
	}

	BoundBonusQuestComponent.Reset();
	Super::NativeDestruct();
}

void UNPBonusRelicWidget::HandleAssignedQuestRelicsChanged()
{
	InitBonusItemList();
}

void UNPBonusRelicWidget::BindToBonusQuestComponent()
{
	if (BoundBonusQuestComponent.IsValid())
	{
		BoundBonusQuestComponent->OnAssignedQuestRelicsChanged.RemoveDynamic(
			this,
			&ThisClass::HandleAssignedQuestRelicsChanged);
		BoundBonusQuestComponent.Reset();
	}

	APlayerController* PlayerController = GetOwningPlayer();
	UNPPlayerBonusQuestComponent* BonusQuestComponent = PlayerController
		? PlayerController->FindComponentByClass<UNPPlayerBonusQuestComponent>()
		: nullptr;
	if (!IsValid(BonusQuestComponent))
	{
		return;
	}

	BoundBonusQuestComponent = BonusQuestComponent;
	BonusQuestComponent->OnAssignedQuestRelicsChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleAssignedQuestRelicsChanged);
}

void UNPBonusRelicWidget::InitBonusItemList()
{
	if (!IsValid(BonusItemVerticalBox) || !IsValid(BonusItemWidgetClass))
	{
		return;
	}

	for (UNPBonusItemWidget* ItemWidget : CreatedItemWidgets)
	{
		if (IsValid(ItemWidget))
		{
			ItemWidget->RemoveFromParent();
		}
	}
	CreatedItemWidgets.Reset();

	const UNPPlayerBonusQuestComponent* BonusQuestComponent = BoundBonusQuestComponent.Get();
	if (!IsValid(BonusQuestComponent))
	{
		return;
	}

	const TArray<ANPBaseRelic*> AssignedQuestRelics = BonusQuestComponent->GetAssignedQuestRelics();
	for (int32 RelicIndex = 0; RelicIndex < AssignedQuestRelics.Num(); ++RelicIndex)
	{
		ANPBaseRelic* Relic = AssignedQuestRelics[RelicIndex];
		const FNPRelicTableRow* RelicData = IsValid(Relic) ? Relic->GetRelicTableData() : nullptr;
		const FString RelicName = RelicData && !RelicData->DisplayName.IsEmpty()
			? RelicData->DisplayName.ToString()	: TEXT("깨진 유물");

		UNPBonusItemWidget* ItemWidget = CreateWidget<UNPBonusItemWidget>(this, BonusItemWidgetClass);
		if (IsValid(ItemWidget))
		{
			ItemWidget->SetItemName(FString::Printf(TEXT("◆ %s"), *RelicName));
			ItemWidget->SetItemColor(UNPShowRelicLocationComponent::GetQuestRelicColor(RelicIndex));
			ItemWidget->SetRelic(Relic);
			BonusItemVerticalBox->AddChildToVerticalBox(ItemWidget);
			CreatedItemWidgets.Add(ItemWidget);
		}
	}
}
