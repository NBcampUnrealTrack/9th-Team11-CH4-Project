#pragma once

#include "CoreMinimal.h"
#include "UI/NPUserWidget.h"
#include "NPBonusItemWidget.generated.h"

class UTextBlock;
class UImage;
class ANPBaseRelic;
class UNPShowRelicLocationComponent;

UCLASS()
class NOPHOTOS_API UNPBonusItemWidget : public UNPUserWidget
{
	GENERATED_BODY()

public:
	UNPBonusItemWidget(const FObjectInitializer& ObjectInitializer);

	void SetItemName(const FString& InItemName);
	void SetItemColor(const FLinearColor& InItemColor);
	void SetRelic(ANPBaseRelic* InRelic);
	void RefreshReturnedState();

protected:
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleRelicReturned();

	void UpdateReturnedState();
	void SetReturnedState(bool bIsReturned);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ItemNameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> StrikeThroughLine;

	TWeakObjectPtr<ANPBaseRelic> AssignedRelic;
	TWeakObjectPtr<UNPShowRelicLocationComponent> BoundRelicLocationComponent;
	FLinearColor ActiveItemColor = FLinearColor::White;
	bool bHasReturnedState = false;
	bool bIsReturned = false;
};
