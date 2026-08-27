#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NPMainPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class UNPPhotoCaptureComponent;
class UNPPhotoFlashWidget;
class UNPPhotoTransferComponent;
class UNPUserWidget;
class UUserWidget;

UCLASS()
class NOPHOTOS_API ANPMainPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ANPMainPlayerController();

	UFUNCTION(BlueprintPure, Category = "Photo")
	UNPPhotoCaptureComponent* GetPhotoCaptureComponent() const { return PhotoCaptureComponent; }

	UFUNCTION(BlueprintPure, Category = "Photo")
	UNPPhotoTransferComponent* GetPhotoTransferComponent() const { return PhotoTransferComponent; }

	void PlayPhotoFlash();

	/** 서버가 확정한 피촬영자 자신의 로컬 화면에서 기존 사진 플래시를 재생합니다. */
	UFUNCTION(Client, Reliable)
	void ClientPlayPhotographedFlash();
	
	UFUNCTION(BlueprintPure, Category = "Room")
	bool IsListenServerHost() const;
	UFUNCTION(BlueprintCallable, Category = "Room")
	void RequestRestartRoom();
	UFUNCTION(BlueprintCallable, Category = "Room")
	void ExitToMainMenu();
	
	UFUNCTION(BlueprintCallable, Category = "UI")
	void ShowGameScreenUI();
	UFUNCTION(Client, Reliable)
	void ClientShowGameScreenUI();
	UFUNCTION(BlueprintCallable, Category = "UI")
	void ShowSelectPictureUI();
	UFUNCTION(Client, Reliable)
	void ClientShowSelectPictureUI();
	UFUNCTION(BlueprintCallable, Category = "UI")
	void ShowResultUI();
	UFUNCTION(Client, Reliable)
	void ClientShowResultUI();

	//서버가 선택 사진과 완료 상태를 확인
	UFUNCTION(Server, Reliable)
	void ServerConfirmPictureSelection(const TArray<FGuid>& SelectedPhotoIds);

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Photo")
	TObjectPtr<UInputAction> TogglePhotoModeAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Photo")
	TObjectPtr<UInputAction> TakePhotoAction;

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Photo|UI")
	TSubclassOf<UNPPhotoFlashWidget> PhotoFlashWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UNPUserWidget> GameScreenWidgetClass;
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UNPUserWidget> SelectPictureWidgetClass;
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UNPUserWidget> ResultWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Room")
	TSoftObjectPtr<UWorld> MainMenuLevel;

private:
	void HandleTogglePhotoModeInput();
	void HandleTakePhotoInput();
	bool ShouldUseTouchControls() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Photo", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNPPhotoCaptureComponent> PhotoCaptureComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Photo", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNPPhotoTransferComponent> PhotoTransferComponent;

	UPROPERTY(Transient)
	TObjectPtr<UNPPhotoFlashWidget> PhotoFlashWidget;

	UFUNCTION(Server, Reliable)
	void ServerRequestRestartRoom();

	void ShowSingleScreen(
		TSubclassOf<UNPUserWidget> WidgetClass);
};
