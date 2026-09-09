#pragma once

#include "CoreMinimal.h"
#include "UI/NPUserWidget.h"
#include "NPLobbyWidget.generated.h"

class UButton;
class UBorder;
class UWidgetAnimation;
class ANPRoomGameState;
class UNPJoinPlayerList;

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
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> StartButton;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> LeaveButton;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> JoinPlayerHoverArea;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UNPJoinPlayerList> JoinPlayerListWidget;
	UPROPERTY(Transient, meta = (BindWidgetAnim))
	TObjectPtr<UWidgetAnimation> JoinPlayers_Collapse;
	UPROPERTY(EditDefaultsOnly, Category = "UI|Join Player List", meta = (ClampMin = "0.0"))
	float PlayerListStableDelay = 1.0f;

	TWeakObjectPtr<ANPRoomGameState> BoundRoomGameState;
	FTimerHandle PlayerListCollapseTimer;
	bool bIsMouseOverJoinPlayerArea = false;
	bool bIsPlayerListStable = false;
	UFUNCTION()
	void RefreshStartButtonVisibility();
	UFUNCTION()
	void OnRoomStateChanged();
	void OnPlayerListChanged();
	void HandlePlayerListBecameStable();
	void UpdateJoinPlayerAreaHover(bool bIsMouseOver);
	void ExpandPlayerList();
	void CollapsePlayerList();
	
	UFUNCTION()
	void OnStartButtonClicked();
	UFUNCTION()
	void OnLeaveClicked();
};
