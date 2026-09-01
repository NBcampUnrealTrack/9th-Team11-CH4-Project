#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NPMainPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class UNPAbilitySystemComponent;
class UNPPhotoCaptureComponent;
class UNPPhotoFlashWidget;
class UNPPhotoTransferComponent;
class UNPNoticeEventWidget;
class UNPUserWidget;
class UUserWidget;
class UNPChatComponent;

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

	UFUNCTION(BlueprintPure, Category = "Chat")
	UNPChatComponent* GetChatComponent() const { return ChatComponent; }

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

	/** 사진 모드와 조준 유물이 함께 사용하는 조준 입력입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Aim and Fire")
	TObjectPtr<UInputAction> AimAction;

	/** 사진 촬영에 사용하는 실행 입력입니다. 유물 발사는 RelicUseAction(F)에서 처리합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Aim and Fire")
	TObjectPtr<UInputAction> FireAction;

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
	TSubclassOf<UNPNoticeEventWidget> NoticeEventWidgetClass;
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UNPUserWidget> SelectPictureWidgetClass;
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UNPUserWidget> ResultWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Room")
	TSoftObjectPtr<UWorld> MainMenuLevel;

private:
	void HandleAimStarted();
	void HandleAimReleased();
	void HandleFireStarted();
	bool IsHoldingAimableRelic() const;
	UNPAbilitySystemComponent* ResolveRelicAbilitySystem() const;
	bool ShouldUseTouchControls() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Photo", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNPPhotoCaptureComponent> PhotoCaptureComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Photo", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNPPhotoTransferComponent> PhotoTransferComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNPChatComponent> ChatComponent;

	UPROPERTY(Transient)
	TObjectPtr<UNPPhotoFlashWidget> PhotoFlashWidget;

	UPROPERTY(Transient)
	TObjectPtr<UNPNoticeEventWidget> NoticeEventWidget;

	UFUNCTION(Server, Reliable)
	void ServerRequestRestartRoom();

	void ShowSingleScreen(TSubclassOf<UNPUserWidget> WidgetClass);
	void EnsureNoticeEventWidget();
};
