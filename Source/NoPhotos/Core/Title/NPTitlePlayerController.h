// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NPTitlePlayerController.generated.h"

class UNPRoomPlayerComponent;
class UNPUserWidget;
class UNPPSOPrecacheComponent;
class UNPMainWorldLoadingWidget;

UCLASS()
class NOPHOTOS_API ANPTitlePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ANPTitlePlayerController();

	UFUNCTION(BlueprintPure, Category = "Room")
	UNPRoomPlayerComponent* GetRoomComponent() const { return RoomComponent; }

	UFUNCTION(BlueprintCallable, Category = "Room")
	bool HostRoom();

	UFUNCTION(BlueprintCallable, Category = "Room")
	bool FindRooms();

	UFUNCTION(BlueprintCallable, Category = "Room")
	bool JoinRoom(int32 RoomNumber);

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ShowMainMenuUI();

	UFUNCTION(Client, Reliable)
	void ClientShowMainMenuUI();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UNPRoomPlayerComponent> RoomComponent;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UNPUserWidget> MainMenuWidgetClass;

private:
	void FinishStartupPreparation();

	UPROPERTY(VisibleAnywhere, Category = "Loading")
	TObjectPtr<UNPPSOPrecacheComponent> PSOPrecacheComponent;

	UPROPERTY(Transient)
	TObjectPtr<UNPMainWorldLoadingWidget> StartupLoadingWidget;

	bool bWaitingForStartupPSO = false;
	bool bStartupPSOReady = false;
};
