#pragma once

#include "CoreMinimal.h"
#include "UI/NPUserWidget.h"
#include "NPJoinPlayerList.generated.h"

class UHorizontalBox;
class UNPJoinPlayer;
class ANPRoomGameState;
class APlayerState;

DECLARE_MULTICAST_DELEGATE(FNPOnJoinPlayerListChanged);

UCLASS()
class NOPHOTOS_API UNPJoinPlayerList : public UNPUserWidget
{
	GENERATED_BODY()
	
public:
	FNPOnJoinPlayerListChanged OnPlayerListChanged;

protected:
	virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> PlayerList;
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UNPJoinPlayer> JoinPlayerWidgetClass;
	UPROPERTY(EditDefaultsOnly, Category = "UI", meta = (ClampMin = "0.0"))
	float InitialJoinAnimationInterval = 0.3f;
	
	FTimerHandle PlayerNameRefreshTimer;
	TWeakObjectPtr<ANPRoomGameState> BoundRoomGameState;
	UPROPERTY(Transient)
	TMap<TObjectPtr<APlayerState>, TObjectPtr<UNPJoinPlayer>> PlayerWidgets;
	UPROPERTY(Transient)
	TSet<TObjectPtr<UNPJoinPlayer>> LeavingPlayerWidgets;
	bool bInitialPlayerPopulationComplete = false;
	int32 InitialJoinAnimationIndex = 0;
	
	UFUNCTION()
	void OnRoomStateChanged();
	void OnPlayerLeaveAnimationFinished(UNPJoinPlayer* PlayerWidget);
	void RefreshPlayerList();
};
