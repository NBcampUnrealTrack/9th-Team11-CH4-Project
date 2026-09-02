#pragma once

#include "CoreMinimal.h"
#include "UI/NPUserWidget.h"
#include "NPLobbyWidget.generated.h"

class UButton;
class ANPRoomGameState;

UCLASS()
class NOPHOTOS_API UNPLobbyWidget : public UNPUserWidget
{
	GENERATED_BODY()

public:
	UNPLobbyWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> StartButton;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> LeaveButton;

	TWeakObjectPtr<ANPRoomGameState> BoundRoomGameState;
	UFUNCTION()
	void RefreshStartButtonVisibility();
	UFUNCTION()
	void OnRoomStateChanged();
	
	UFUNCTION()
	void OnStartButtonClicked();
	UFUNCTION()
	void OnLeaveClicked();
};
