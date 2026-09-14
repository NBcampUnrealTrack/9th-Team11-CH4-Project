#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "NPMainPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class UNPAbilitySystemComponent;
class UNPPhotoCaptureComponent;
class UNPPhotoFlashWidget;
class UNPPhotoTransferComponent;
class UNPNoticeEventWidget;
class UNPMainWorldLoadingWidget;
class UNPAimCrosshairWidget;
class UNPRelicUsePromptWidget;
class UNPUserWidget;
class UUserWidget;
class UNPChatComponent;
struct FInputKeyEventArgs;

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

	/** Level Instance 준비 중 수평 이동 입력이 잠겨 있는지 반환합니다. 점프는 이 상태와 무관합니다. */
	UFUNCTION(BlueprintPure, Category = "Room|Loading")
	bool IsMainWorldInputLocked() const { return bMainWorldInputLocked; }

	UFUNCTION(BlueprintCallable, Category = "Room")
	void RequestRestartRoom();
	UFUNCTION(BlueprintCallable, Category = "Room")
	void ExitToMainMenu();
	
	UFUNCTION(BlueprintCallable, Category = "UI")
	void ShowGameScreenUI();
	UFUNCTION(Client, Reliable)
	void ClientShowGameScreenUI();

	/** 메인 월드의 방 스트리밍이 끝날 때까지 로컬 입력과 로딩 화면을 유지합니다. */
	UFUNCTION(Client, Reliable)
	void ClientBeginMainWorldPreparation();

	/** 서버가 전 플레이어 준비를 확인한 뒤 로딩 화면을 닫고 조작을 허용합니다. */
	UFUNCTION(Client, Reliable)
	void ClientFinishMainWorldPreparation();

	UFUNCTION(Client, Reliable)
	void ClientNotifyMainWorldLoadFailed();

	UFUNCTION(Server, Reliable)
	void ServerReportMainWorldReady();
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

	UFUNCTION(Server, Reliable)
	void ServerLikeResultPhoto(FGuid PhotoId);
	void HandleSelectedPhotoStored(const FGuid& PhotoId);

	/** 개발 빌드에서 현재 Pawn의 Grab 입력을 창 포커스와 무관하게 유지합니다. */
	UFUNCTION(Exec)
	void NPTestLockGrab();

	/** 개발 빌드에서 Grab 입력 고정을 해제하고 현재 물체를 놓습니다. */
	UFUNCTION(Exec)
	void NPTestUnlockGrab();

	/** 개발 빌드에서 현재 Pawn의 Grab 및 보유 유물 상태를 로그로 출력합니다. */
	UFUNCTION(Exec)
	void NPTestPrintGrabState();

	UFUNCTION(Server, Reliable)
	void ServerAddCheatPoint();

	UFUNCTION(Server, Reliable)
	void ServerRemoveCheatPoint();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnRep_Pawn() override;
	virtual void SetupInputComponent() override;
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;

	/** 사진 모드와 조준 유물이 함께 사용하는 조준 입력입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Aim and Fire")
	TObjectPtr<UInputAction> AimAction;

	/** 사진 촬영과 조준 유물 발사가 함께 사용하는 실행 입력입니다. */
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

	//옵션 위젯 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Option")
	TSubclassOf<UNPUserWidget> OptionPanelWidgetClass;

	/** 맵 로딩 이후 방 Level Instance와 다른 플레이어 준비를 기다리는 UMG입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Loading")
	TSubclassOf<UNPMainWorldLoadingWidget> MainWorldLoadingWidgetClass;

	/** 방 로딩이 빨리 끝나더라도 메인 월드 로딩 화면을 유지할 최소 시간입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Loading",
		meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float MinimumMainWorldLoadingDisplaySeconds = 1.0f;

	/** State.Relic.Aiming 태그가 활성화된 동안 로컬 화면에 표시할 조준점 위젯입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Aim")
	TSubclassOf<UNPAimCrosshairWidget> AimCrosshairWidgetClass;

	/** State.Photo.Aiming 태그가 활성화된 동안 로컬 화면에 표시할 사진 조준 UI입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Aim")
	TSubclassOf<UNPAimCrosshairWidget> PhotoAimWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Room")
	TSoftObjectPtr<UWorld> MainMenuLevel;

private:
	UFUNCTION(Client, Reliable)
	void ClientUploadSelectedPhotos(const TArray<FGuid>& SelectedPhotoIds);

	UFUNCTION(Server, Reliable)
	void ServerReportSelectedPhotoUploadFailed(FGuid PhotoId);

	void TryCompletePictureSelection();

	bool ShouldBypassRoomPreparationForEditorTest() const;
	void BeginLocalMainWorldPreparation();

	UFUNCTION()
	void HandleLocalRoomGenerationCompleted();

	UFUNCTION()
	void HandleLocalRoomGenerationFailed();
	void CompleteLocalMainWorldReadiness();

	void BindRoomGenerationState();
	void SetMainWorldInputLocked(bool bLocked);
	void ShowMainWorldLoadingOverlay();
	void FinishTransitionLoadingScreenHandoff();
	void HideMainWorldLoadingOverlay();
	void ShowMainWorldLoadingFailure();

	void HandleAimStarted();
	void HandleAimReleased();
	void HandleFireStarted();
	void ToggleOptionPanel();
	void BindAimCrosshairToAbilitySystem();
	void UnbindAimCrosshairFromAbilitySystem();
	void HandleRelicAimingTagChanged(const FGameplayTag Tag, int32 NewCount);
	void HandlePhotoAimingTagChanged(const FGameplayTag Tag, int32 NewCount);
	void HandlePhotoCooldownTagChanged(const FGameplayTag Tag, int32 NewCount);
	void SetAimCrosshairActive(bool bActive);
	void SetPhotoAimWidgetActive(bool bActive);
	void BeginPhotoCooldownDisplayUpdates();
	void StopPhotoCooldownDisplayUpdates(bool bShowFullyCharged);
	void UpdatePhotoCooldownDisplay();
	void UpdateRelicUsePrompt();
	bool IsHoldingAimableRelic() const;
	UNPAbilitySystemComponent* ResolveAbilitySystem() const;
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

	UPROPERTY(Transient)
	TObjectPtr<UNPMainWorldLoadingWidget> MainWorldLoadingWidget;

	UPROPERTY(Transient)
	TObjectPtr<UNPAimCrosshairWidget> AimCrosshairWidget;

	UPROPERTY(Transient)
	TObjectPtr<UNPAimCrosshairWidget> PhotoAimWidget;

	/** 사용 가능한 유물을 오른손에 들었을 때 표시할 F키 및 원형 쿨타임 위젯입니다. */
	UPROPERTY(EditDefaultsOnly, Category="UI|Relic Use")
	TSubclassOf<UNPRelicUsePromptWidget> RelicUsePromptWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UNPRelicUsePromptWidget> RelicUsePromptWidget;

	/** 보유 유물 및 쿨타임 UI의 로컬 갱신 간격입니다. */
	UPROPERTY(EditDefaultsOnly, Category="UI|Relic Use",
		meta=(ClampMin="0.02", UIMin="0.02", Units="s"))
	float RelicUsePromptUpdateInterval = 0.05f;

	TWeakObjectPtr<UNPAbilitySystemComponent> AimCrosshairAbilitySystem;
	FDelegateHandle RelicAimingTagChangedHandle;
	FDelegateHandle PhotoAimingTagChangedHandle;
	FDelegateHandle PhotoCooldownTagChangedHandle;
	FTimerHandle PhotoCooldownDisplayTimer;
	FTimerHandle RelicUsePromptUpdateTimer;

	/** WBP_NPPhotoAim의 배터리를 구성하는 충전 칸 수입니다. */
	UPROPERTY(EditDefaultsOnly, Category = "UI|Aim", meta = (ClampMin = "1", UIMin = "1"))
	int32 PhotoCooldownCellCount = 5;

	/** 활성 쿨다운 Effect의 남은 시간을 UI에 반영하는 로컬 갱신 간격입니다. */
	UPROPERTY(EditDefaultsOnly, Category = "UI|Aim", meta = (ClampMin = "0.02", UIMin = "0.02", Units = "s"))
	float PhotoCooldownDisplayUpdateInterval = 0.1f;

	bool bMainWorldInputLocked = false;
	bool bReportedMainWorldReady = false;
	double MainWorldLoadingShownAtRealTime = -1.0;
	FTimerHandle MinimumMainWorldLoadingTimer;
	TSet<FGuid> PendingSelectedPhotoIds;
	bool bTransitionLoadingScreenHandoffScheduled = false;

	UFUNCTION(Server, Reliable)
	void ServerRequestRestartRoom();

	void ShowSingleScreen(TSubclassOf<UNPUserWidget> WidgetClass);
	void EnsureNoticeEventWidget();
};
