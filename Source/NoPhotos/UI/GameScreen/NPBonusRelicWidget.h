#pragma once

#include "CoreMinimal.h"
#include "UI/NPUserWidget.h"
#include "NPBonusRelicWidget.generated.h"

class UVerticalBox;
class UNPBonusItemWidget;
class UNPPlayerBonusQuestComponent;

UCLASS()
class NOPHOTOS_API UNPBonusRelicWidget : public UNPUserWidget
{
	GENERATED_BODY()

public:
	UNPBonusRelicWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> BonusItemVerticalBox;

	UPROPERTY(EditAnywhere, Category = "UI|BonusRelic")
	TSubclassOf<UNPBonusItemWidget> BonusItemWidgetClass;

	TWeakObjectPtr<UNPPlayerBonusQuestComponent> BoundBonusQuestComponent;
	TArray<TObjectPtr<UNPBonusItemWidget>> CreatedItemWidgets;

	UFUNCTION()
	void HandleAssignedQuestRelicsChanged();
	void BindToBonusQuestComponent();
	void InitBonusItemList();
};
