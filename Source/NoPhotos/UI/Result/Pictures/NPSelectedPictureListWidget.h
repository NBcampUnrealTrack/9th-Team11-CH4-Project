//화면 왼쪽 아래에 뜨는 작은 이미지 리스트

#pragma once

#include "CoreMinimal.h"
#include "UI/NPUserWidget.h"
#include "NPSelectedPictureListWidget.generated.h"

class UHorizontalBox;
class UTexture2D;
class UNPSelectedPictureWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNPSelectedPictureListItemClicked, int32, PictureIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNPSelectedPictureListItemRemoveRequested, int32, PictureIndex);

UCLASS()
class NOPHOTOS_API UNPSelectedPictureListWidget : public UNPUserWidget
{
	GENERATED_BODY()

public:
	void AddSelectedPicture(UTexture2D* InTexture, int32 PictureIndex);
	void RemoveSelectedPicture(int32 PictureIndex);
	void ClearSelectedPictures();
	void SetRemovalEnabled(bool bEnabled);

	UPROPERTY(BlueprintAssignable, Category = "Picture")
	FNPSelectedPictureListItemClicked OnPictureClicked;

	UPROPERTY(BlueprintAssignable, Category = "Picture")
	FNPSelectedPictureListItemRemoveRequested OnPictureRemoveRequested;

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(EditDefaultsOnly, Category = "Picture")
	TSubclassOf<UNPSelectedPictureWidget> SelectedPictureWidgetClass;

private:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UHorizontalBox> SelectedPictureList;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UNPSelectedPictureWidget>> SelectedPictureWidgets;
	UPROPERTY(Transient)
	TArray<int32> PictureIndices;
	bool bRemovalEnabled = true;

	UFUNCTION()
	void HandlePictureWidgetClicked(UNPSelectedPictureWidget* PictureWidget);
	UFUNCTION()
	void HandlePictureWidgetRemoveRequested(UNPSelectedPictureWidget* PictureWidget);
};
