#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"
#include "UI/NPUserWidget.h"
#include "NPRoomListWidget.generated.h"

class UScrollBox;
class UButton;
class UNPRoomItemWidget;

UCLASS()
class NOPHOTOS_API UNPRoomListWidget : public UNPUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 방 목록을 스크롤 박스에 동적으로 채움
	UFUNCTION(BlueprintCallable, Category = "Room")
	void RefreshRoomList();

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> RoomListScrollBox;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> RefreshButton;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UNPRoomItemWidget> RoomItemClass;

	UPROPERTY(EditDefaultsOnly, Category = "UI|Room Search")
	FSlateFontInfo SearchingTextFont;
	UPROPERTY(EditDefaultsOnly, Category = "UI|Room Search")
	FLinearColor SearchingTextColor = FLinearColor::White;

	UFUNCTION()
	void OnRefreshClicked();
	UFUNCTION()
	void OnCloseClicked();
	UFUNCTION()
	void OnFindRoomsComplete(const TArray<int32>& RoomIndices);
	void UpdateSearchState(bool bInIsSearching);
	UFUNCTION()
	void OnRoomItemSelected(int32 SelectedRoomNumber);

	bool bIsSearching = false;
};
