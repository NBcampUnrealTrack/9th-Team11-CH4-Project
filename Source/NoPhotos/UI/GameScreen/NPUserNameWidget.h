#pragma once

#include "CoreMinimal.h"
#include "UI/NPUserWidget.h"
#include "NPUserNameWidget.generated.h"

class APlayerState;
class UImage;
class UTextBlock;

UCLASS()
class NOPHOTOS_API UNPUserNameWidget : public UNPUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Nameplate")
	void SetTargetPlayerState(APlayerState* InPlayerState);
	void SetPhotoTargetIndicatorVisible(bool bVisible);

protected:
	virtual void NativeConstruct() override;

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> UserNameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> PhotoTargetIcon;
	UPROPERTY(Transient)
	TObjectPtr<APlayerState> TargetPlayerState;
	
	void RefreshPlayerName();
};
