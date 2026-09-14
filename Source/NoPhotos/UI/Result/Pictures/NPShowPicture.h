//큰 사진

#pragma once

#include "CoreMinimal.h"
#include "Iris/Core/IrisProfiler.h"
#include "UI/NPUserWidget.h"
#include "NPShowPicture.generated.h"

class UButton;
class UImage;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNPShowPictureSelectRequested,	int32,	PictureIndex);
UCLASS()
class NOPHOTOS_API UNPShowPicture : public UNPUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Picture")
	FNPShowPictureSelectRequested OnSelectRequested;
	
	void SetPicture(UTexture2D* InTexture, int32 InPictureIndex);
	void SetSelected(bool InSelected);
	void SetSelectionEnabled(bool bEnabled);

protected:
	virtual void NativeConstruct() override;

private:
	UFUNCTION()
	void OnSelectButtonClicked();

	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool IsSelected = false;

	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	int32 CurrentPictureIndex = INDEX_NONE;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> SelectPicture;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> SelectButton;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> StampIcon;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Picture|Stamp", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTexture2D> UnselectedStampTexture;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Picture|Stamp", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTexture2D> SelectedStampTexture;
};
