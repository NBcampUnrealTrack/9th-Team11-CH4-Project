// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NPRoomPlayerController.generated.h"

class UNPRoomPlayerComponent;
class UNPUserWidget;
class UUserWidget;
class UInputMappingContext;
class UInputAction;
class UNPChatComponent;

/** 대기방의 방 기능, UI와 입력을 담당하는 PlayerController입니다. */
UCLASS()
class NOPHOTOS_API ANPRoomPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ANPRoomPlayerController();

	UFUNCTION(BlueprintPure, Category = "Room")
	UNPRoomPlayerComponent* GetRoomComponent() const { return RoomComponent; }

	UFUNCTION(BlueprintPure, Category = "Chat")
	UNPChatComponent* GetChatComponent() const { return ChatComponent; }

	UFUNCTION(BlueprintCallable, Category = "Room")
	void RequestStartGame();

	UFUNCTION(BlueprintCallable, Category = "Room")
	void RequestRestartRoom();

	UFUNCTION(BlueprintCallable, Category = "Room")
	void ExitRoom();

	UFUNCTION(BlueprintCallable, Category = "Room")
	void ShowRoomUsers() const;

	UFUNCTION(BlueprintPure, Category = "Room")
	bool IsRoomHost() const;

	UFUNCTION(BlueprintPure, Category = "Room")
	bool CanStartGame() const;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputMappingContext* InputMappingContext;
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* ChangeInputAction;

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ShowLobbyUI();

	UFUNCTION(Client, Reliable)
	void ClientShowLobbyUI();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UNPRoomPlayerComponent> RoomComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UNPChatComponent> ChatComponent;

	UPROPERTY(EditAnywhere, Category = "Input|Input Mappings")
	TArray<TObjectPtr<UInputMappingContext>> DefaultMappingContexts;

	UPROPERTY(EditAnywhere, Category = "Input|Input Mappings")
	TArray<TObjectPtr<UInputMappingContext>> MobileExcludedMappingContexts;

	UPROPERTY(EditAnywhere, Category = "Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UNPUserWidget> LobbyWidgetClass;

	bool bIsMouseInput = false;
	void ChangeInputMode();

private:
	bool ShouldUseTouchControls() const;
	void SetCharacterInputMappingEnabled(bool bEnabled);
	void SetLobbyInputMappingEnabled(bool bEnabled);
	void ApplyLobbyInputMode();
	void ShowSingleScreen(TSubclassOf<UNPUserWidget> WidgetClass);
};
