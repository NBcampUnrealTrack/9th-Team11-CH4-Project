#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "NPRoomPlayerController.generated.h"

class UNPRoomPlayerComponent;
class UNPAbilitySystemComponent;
class UNPAimCrosshairWidget;
class UNPPhotoCaptureComponent;
class UNPPhotoFlashWidget;
class UNPUserWidget;
class UUserWidget;
class UInputMappingContext;
class UInputAction;
class UNPChatComponent;
struct FInputKeyEventArgs;

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

	UFUNCTION(BlueprintPure, Category="Photo")
	UNPPhotoCaptureComponent* GetPhotoCaptureComponent() const
	{
		return PhotoCaptureComponent;
	}

	void PlayPhotoFlash();
	void PlayPresentationOnlyShutterCue();

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
	
	void ToggleLobbyInputMode();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnRep_Pawn() override;
	virtual void SetupInputComponent() override;
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UNPRoomPlayerComponent> RoomComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UNPChatComponent> ChatComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UNPPhotoCaptureComponent> PhotoCaptureComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Photo")
	TObjectPtr<UInputAction> AimAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Photo")
	TObjectPtr<UInputAction> FireAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI|Photo")
	TSubclassOf<UNPAimCrosshairWidget> PhotoAimWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI|Photo")
	TSubclassOf<UNPPhotoFlashWidget> PhotoFlashWidgetClass;

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Option")
	TSubclassOf<UNPUserWidget> OptionPanelWidgetClass;

	bool bIsMouseInput = false;

private:
	void HandlePhotoAimStarted();
	void HandlePhotoFireStarted();
	void BindPhotoUIToAbilitySystem();
	void UnbindPhotoUIFromAbilitySystem();
	void HandlePhotoAimingTagChanged(const FGameplayTag Tag, int32 NewCount);
	void HandlePhotoCooldownTagChanged(const FGameplayTag Tag, int32 NewCount);
	void SetPhotoAimWidgetActive(bool bActive);
	void BeginPhotoCooldownDisplayUpdates();
	void StopPhotoCooldownDisplayUpdates(bool bShowFullyCharged);
	void UpdatePhotoCooldownDisplay();
	UNPAbilitySystemComponent* ResolveAbilitySystem() const;
	bool ShouldUseTouchControls() const;
	void SetCharacterInputMappingEnabled(bool bEnabled);
	void SetLobbyInputMappingEnabled(bool bEnabled);
	void ApplyLobbyInputMode();
	void ToggleOptionPanel();
	void ShowSingleScreen(TSubclassOf<UNPUserWidget> WidgetClass);

	UPROPERTY(Transient)
	TObjectPtr<UNPAimCrosshairWidget> PhotoAimWidget;

	UPROPERTY(Transient)
	TObjectPtr<UNPPhotoFlashWidget> PhotoFlashWidget;

	TWeakObjectPtr<UNPAbilitySystemComponent> PhotoUIAbilitySystem;
	FDelegateHandle PhotoAimingTagChangedHandle;
	FDelegateHandle PhotoCooldownTagChangedHandle;
	FTimerHandle PhotoCooldownDisplayTimer;

	UPROPERTY(EditDefaultsOnly, Category="UI|Photo",
		meta=(ClampMin="1", UIMin="1"))
	int32 PhotoCooldownCellCount = 5;

	UPROPERTY(EditDefaultsOnly, Category="UI|Photo",
		meta=(ClampMin="0.02", UIMin="0.02", Units="s"))
	float PhotoCooldownDisplayUpdateInterval = 0.1f;
};
