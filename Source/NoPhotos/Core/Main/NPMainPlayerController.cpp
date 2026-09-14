#include "Core/Main/NPMainPlayerController.h"

#include "AsyncLoadingScreenLibrary.h"
#include "Core/Main/NPMainGameMode.h"
#include "Core/Main/NPMainGameState.h"
#include "Core/NPPlayerState.h"
#include "Core/GameplayTag/NPGameplayTags.h"
#include "Core/Chat/NPChatComponent.h"
#include "Core/Room/NPRoomCheatManager.h"
#include "Core/Room/NPRoomSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Components/PrimitiveComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "InputCoreTypes.h"
#include "GameplayEffect.h"
#include "Gameplay/Photo/NPPhotoCaptureComponent.h"
#include "Gameplay/Photo/NPPhotoFlashWidget.h"
#include "Gameplay/Photo/NPPhotoLog.h"
#include "Gameplay/Photo/NPPhotoRepository.h"
#include "Gameplay/Photo/NPPhotoTransferComponent.h"
#include "Gameplay/AbilitySystem/NPAbilitySystemComponent.h"
#include "Gameplay/Character/Component/NPStablePhysicsGrabComponent.h"
#include "Gameplay/Character/NPReplicatedStablePhysicsPawn.h"
#include "Gameplay/Relic/Components/NPAimableRelicComponent.h"
#include "Gameplay/Relic/Components/NPThrowableRelicComponent.h"
#include "Gameplay/Relic/Components/NPUsableRelicComponent.h"
#include "Gameplay/Map/Room/NPRoomGenerationHelper.h"
#include "NoPhotos.h"
#include "SubSystem/NPUIManagerSubsystem.h"
#include "SubSystem/Room/NPRoomGenerateSubsystem.h"
#include "UI/GameScreen/Event/NPNoticeEventWidget.h"
#include "UI/GameScreen/NPAimCrosshairWidget.h"
#include "UI/GameScreen/Relic/NPRelicUsePromptUIComponent.h"
#include "UI/Loading/NPMainWorldLoadingWidget.h"
#include "UI/NPUserWidget.h"
#include "UObject/ConstructorHelpers.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "HAL/PlatformTime.h"
#include "TimerManager.h"

ANPMainPlayerController::ANPMainPlayerController()
{
	PhotoCaptureComponent = CreateDefaultSubobject<UNPPhotoCaptureComponent>(TEXT("PhotoCaptureComponent"));
	PhotoTransferComponent = CreateDefaultSubobject<UNPPhotoTransferComponent>(TEXT("PhotoTransferComponent"));
	ChatComponent = CreateDefaultSubobject<UNPChatComponent>(TEXT("ChatComponent"));
	RelicUsePromptUIComponent = CreateDefaultSubobject<UNPRelicUsePromptUIComponent>(
		TEXT("RelicUsePromptUIComponent"));
	CheatClass = UNPRoomCheatManager::StaticClass();

	static ConstructorHelpers::FObjectFinder<UInputMappingContext>
	DefaultMapping(TEXT("/Game/Input/IMC_Default.IMC_Default"));

	static ConstructorHelpers::FObjectFinder<UInputMappingContext>
		MouseLookMapping(TEXT("/Game/Input/IMC_MouseLook.IMC_MouseLook"));

	static ConstructorHelpers::FObjectFinder<UInputAction>
		AimInputAction(TEXT("/Game/Input/Actions/IA_Aim.IA_Aim"));
	static ConstructorHelpers::FObjectFinder<UInputAction>
		LegacyPhotoModeAction(
			TEXT("/Game/Input/Actions/IA_PhotoMode.IA_PhotoMode"));

	static ConstructorHelpers::FObjectFinder<UInputAction>
		FireInputAction(TEXT("/Game/Input/Actions/IA_Fire.IA_Fire"));
	static ConstructorHelpers::FObjectFinder<UInputAction>
		LegacyPhotoShotAction(
			TEXT("/Game/Input/Actions/IA_PhotoShot.IA_PhotoShot"));

	if (DefaultMapping.Succeeded())
	{
		DefaultMappingContexts.Add(DefaultMapping.Object);
	}

	if (MouseLookMapping.Succeeded())
	{
		DefaultMappingContexts.Add(MouseLookMapping.Object);
	}

	if (AimInputAction.Succeeded())
	{
		AimAction = AimInputAction.Object;
	}
	else if (LegacyPhotoModeAction.Succeeded())
	{
		AimAction = LegacyPhotoModeAction.Object;
	}

	if (FireInputAction.Succeeded())
	{
		FireAction = FireInputAction.Object;
	}
	else if (LegacyPhotoShotAction.Succeeded())
	{
		FireAction = LegacyPhotoShotAction.Object;
	}
}

void ANPMainPlayerController::ServerAddCheatPoint_Implementation()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (ANPPlayerState* NPPlayerState = GetPlayerState<ANPPlayerState>())
	{
		NPPlayerState->AddScore(100);
	}
#endif
}

void ANPMainPlayerController::ServerRemoveCheatPoint_Implementation()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (ANPPlayerState* NPPlayerState = GetPlayerState<ANPPlayerState>())
	{
		NPPlayerState->AddScore(-100);
	}
#endif
}

void ANPMainPlayerController::NPTestLockGrab()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	ANPReplicatedStablePhysicsPawn* StablePawn =
		GetPawn<ANPReplicatedStablePhysicsPawn>();
	if (!IsValid(StablePawn))
	{
		UE_LOG(
			LogNoPhotos,
			Warning,
			TEXT("[PhotoTest] Grab lock failed: invalid replicated physics Pawn. Controller=%s"),
			*GetNameSafe(this));
		return;
	}

	StablePawn->SetDebugGrabLocked(true);
	UE_LOG(
		LogNoPhotos,
		Log,
		TEXT("[PhotoTest] Grab lock enabled. Pawn=%s"),
		*GetNameSafe(StablePawn));
#endif
}

void ANPMainPlayerController::NPTestUnlockGrab()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	ANPReplicatedStablePhysicsPawn* StablePawn =
		GetPawn<ANPReplicatedStablePhysicsPawn>();
	if (!IsValid(StablePawn))
	{
		return;
	}

	StablePawn->SetDebugGrabLocked(false);
	UE_LOG(
		LogNoPhotos,
		Log,
		TEXT("[PhotoTest] Grab lock disabled. Pawn=%s"),
		*GetNameSafe(StablePawn));
#endif
}

void ANPMainPlayerController::NPTestPrintGrabState()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	const ANPReplicatedStablePhysicsPawn* StablePawn =
		GetPawn<ANPReplicatedStablePhysicsPawn>();
	if (!IsValid(StablePawn))
	{
		UE_LOG(
			LogNoPhotos,
			Warning,
			TEXT("[PhotoTest] Grab state unavailable: invalid replicated physics Pawn. Controller=%s"),
			*GetNameSafe(this));
		return;
	}

	UE_LOG(
		LogNoPhotos,
		Log,
		TEXT("[PhotoTest] Grab state. Pawn=%s Locked=%s ReplicatedActive=%s HeldRelic=%s"),
		*GetNameSafe(StablePawn),
		StablePawn->IsDebugGrabLocked() ? TEXT("true") : TEXT("false"),
		StablePawn->IsReplicatedGrabActive() ? TEXT("true") : TEXT("false"),
		*GetNameSafe(StablePawn->GetHeldRelic_Implementation()));
#endif
}

void ANPMainPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
		BeginLocalMainWorldPreparation();

		if (AimCrosshairWidgetClass)
		{
			AimCrosshairWidget = CreateWidget<UNPAimCrosshairWidget>(
				this,
				AimCrosshairWidgetClass);
			if (IsValid(AimCrosshairWidget))
			{
				AimCrosshairWidget->AddToPlayerScreen(50);
				AimCrosshairWidget->SetAimActive(false);
			}
		}

		if (PhotoAimWidgetClass)
		{
			PhotoAimWidget = CreateWidget<UNPAimCrosshairWidget>(
				this,
				PhotoAimWidgetClass);
			if (IsValid(PhotoAimWidget))
			{
				PhotoAimWidget->AddToPlayerScreen(50);
				PhotoAimWidget->SetAimActive(false);
			}
		}
		BindAimCrosshairToAbilitySystem();

		if (PhotoFlashWidgetClass)
		{
			PhotoFlashWidget = CreateWidget<UNPPhotoFlashWidget>(this, PhotoFlashWidgetClass);
			if (PhotoFlashWidget)
			{
				PhotoFlashWidget->AddToPlayerScreen(100);
				PhotoFlashWidget->SetVisibility(ESlateVisibility::Collapsed);
				UE_LOG(LogNPPhoto, Log, TEXT("[PhotoUI] Flash widget created. Widget=%s"), *GetNameSafe(PhotoFlashWidget));
			}
		}
		else
		{
			UE_LOG(LogNPPhoto, Warning, TEXT("[PhotoUI] PhotoFlashWidgetClass is not assigned."));
		}
	}

	if (ShouldUseTouchControls() && IsLocalPlayerController())
	{
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);
		if (MobileControlsWidget)
		{
			MobileControlsWidget->AddToPlayerScreen(0);
		}
		else
		{
			UE_LOG(LogNoPhotos, Error, TEXT("Could not spawn mobile controls widget."));
		}
	}
}

void ANPMainPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindAimCrosshairFromAbilitySystem();
	Super::EndPlay(EndPlayReason);
}

void ANPMainPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	BindAimCrosshairToAbilitySystem();
}

void ANPMainPlayerController::OnRep_Pawn()
{
	Super::OnRep_Pawn();
	BindAimCrosshairToAbilitySystem();
}

void ANPMainPlayerController::ClientBeginMainWorldPreparation_Implementation()
{
	if (!IsLocalController())
	{
		return;
	}

	bReportedMainWorldReady = false;
	if (IsValid(PhotoCaptureComponent))
	{
		PhotoCaptureComponent->ResetLocalPhotos();
	}
	BeginLocalMainWorldPreparation();
}

void ANPMainPlayerController::BeginLocalMainWorldPreparation()
{
	if (ShouldBypassRoomPreparationForEditorTest())
	{
		GetWorldTimerManager().ClearTimer(MinimumMainWorldLoadingTimer);
		SetMainWorldInputLocked(false);
		HideMainWorldLoadingOverlay();
		CompleteLocalMainWorldReadiness();
		UE_LOG(
			LogNoPhotos,
			Log,
			TEXT("[MainWorldLoading] Editor level-instance test bypass enabled. Controller=%s"),
			*GetNameSafe(this));
		return;
	}

	SetMainWorldInputLocked(true);
	ShowMainWorldLoadingOverlay();
	BindRoomGenerationState();
}

bool ANPMainPlayerController::ShouldBypassRoomPreparationForEditorTest() const
{
#if WITH_EDITOR
	UWorld* World = GetWorld();
	if (!World || World->WorldType != EWorldType::PIE)
	{
		return false;
	}

	TActorIterator<ANPRoomGenerationHelper> RoomGenerationHelperIterator(World);
	const bool bHasRoomGenerationHelper =
		static_cast<bool>(RoomGenerationHelperIterator);
	if (bHasRoomGenerationHelper)
	{
		return false;
	}

	return true;
#else
	return false;
#endif
}

void ANPMainPlayerController::BindRoomGenerationState()
{
	UNPRoomGenerateSubsystem* RoomGenerator = GetWorld()
		? GetWorld()->GetSubsystem<UNPRoomGenerateSubsystem>()
		: nullptr;
	if (!RoomGenerator)
	{
		return;
	}

	RoomGenerator->OnRoomGenerationCompleted.AddUniqueDynamic(
		this,
		&ThisClass::HandleLocalRoomGenerationCompleted);
	RoomGenerator->OnRoomGenerationFailed.AddUniqueDynamic(
		this,
		&ThisClass::HandleLocalRoomGenerationFailed);

	if (RoomGenerator->IsGenerationComplete())
	{
		HandleLocalRoomGenerationCompleted();
	}
	else if (RoomGenerator->HasGenerationFailed())
	{
		HandleLocalRoomGenerationFailed();
	}
}

void ANPMainPlayerController::HandleLocalRoomGenerationCompleted()
{
	if (!IsLocalController() || bReportedMainWorldReady)
	{
		return;
	}

	const double CurrentRealTime = FPlatformTime::Seconds();
	double ElapsedDisplayTime = 0.0;
	if (MainWorldLoadingShownAtRealTime >= 0.0)
	{
		ElapsedDisplayTime = CurrentRealTime - MainWorldLoadingShownAtRealTime;
	}
	const float RemainingDisplayTime = FMath::Max(
		0.0f,
		MinimumMainWorldLoadingDisplaySeconds - static_cast<float>(ElapsedDisplayTime));
	if (RemainingDisplayTime > 0.0f)
	{
		GetWorldTimerManager().SetTimer(
			MinimumMainWorldLoadingTimer,
			this,
			&ThisClass::CompleteLocalMainWorldReadiness,
			RemainingDisplayTime,
			false);
		return;
	}

	CompleteLocalMainWorldReadiness();
}

void ANPMainPlayerController::CompleteLocalMainWorldReadiness()
{
	if (!IsLocalController() || bReportedMainWorldReady)
	{
		return;
	}

	bReportedMainWorldReady = true;
	ServerReportMainWorldReady();
}

void ANPMainPlayerController::HandleLocalRoomGenerationFailed()
{
	if (IsLocalController())
	{
		GetWorldTimerManager().ClearTimer(MinimumMainWorldLoadingTimer);
		UE_LOG(LogNoPhotos, Error,
			TEXT("[MainWorldLoading] Local room generation failed. Controller=%s"),
			*GetNameSafe(this));
	}
}

void ANPMainPlayerController::ServerReportMainWorldReady_Implementation()
{
	if (ANPMainGameMode* MainGameMode = GetWorld()
		? GetWorld()->GetAuthGameMode<ANPMainGameMode>()
		: nullptr)
	{
		MainGameMode->RegisterPlayerWorldReady(this);
	}
}

void ANPMainPlayerController::ClientFinishMainWorldPreparation_Implementation()
{
	GetWorldTimerManager().ClearTimer(MinimumMainWorldLoadingTimer);
	SetMainWorldInputLocked(false);
	HideMainWorldLoadingOverlay();
	ShowGameScreenUI();
}

void ANPMainPlayerController::ClientNotifyMainWorldLoadFailed_Implementation()
{
	SetMainWorldInputLocked(true);
	ShowMainWorldLoadingFailure();
	UE_LOG(LogNoPhotos, Error,
		TEXT("[MainWorldLoading] Server reported main world load failure."));
}

void ANPMainPlayerController::SetMainWorldInputLocked(const bool bLocked)
{
	if (bMainWorldInputLocked == bLocked)
	{
		return;
	}

	bMainWorldInputLocked = bLocked;
	SetIgnoreMoveInput(bLocked);
	SetIgnoreLookInput(bLocked);

	if (bLocked)
	{
		if (UNPAbilitySystemComponent* AbilitySystem = ResolveAbilitySystem())
		{
			AbilitySystem->CancelPhotoAimAbility();
		}

		if (ANPStablePhysicsPawn* StablePawn =
			Cast<ANPStablePhysicsPawn>(GetPawn()))
		{
			StablePawn->StopMovementInput();
		}
	}
}

void ANPMainPlayerController::ShowMainWorldLoadingOverlay()
{
	if (!IsLocalController())
	{
		return;
	}

	if (!IsValid(MainWorldLoadingWidget))
	{
		TSubclassOf<UNPMainWorldLoadingWidget> WidgetClass =
			MainWorldLoadingWidgetClass;
		if (!WidgetClass)
		{
			WidgetClass = UNPMainWorldLoadingWidget::StaticClass();
		}
		MainWorldLoadingWidget =
			CreateWidget<UNPMainWorldLoadingWidget>(this, WidgetClass);
		if (IsValid(MainWorldLoadingWidget))
		{
			MainWorldLoadingWidget->AddToPlayerScreen(10000);
			MainWorldLoadingShownAtRealTime = FPlatformTime::Seconds();
		}
	}

	if (IsValid(MainWorldLoadingWidget))
	{
		MainWorldLoadingWidget->ShowLoading();
	}

	if (!bTransitionLoadingScreenHandoffScheduled)
	{
		bTransitionLoadingScreenHandoffScheduled = true;
		GetWorldTimerManager().SetTimerForNextTick(
			this,
			&ThisClass::FinishTransitionLoadingScreenHandoff);
	}
}

void ANPMainPlayerController::FinishTransitionLoadingScreenHandoff()
{
	bTransitionLoadingScreenHandoffScheduled = false;
	UAsyncLoadingScreenLibrary::HideTransitionHandoffOverlay();
}

void ANPMainPlayerController::HideMainWorldLoadingOverlay()
{
	if (!IsValid(MainWorldLoadingWidget))
	{
		return;
	}

	MainWorldLoadingWidget->RemoveFromParent();
	MainWorldLoadingWidget = nullptr;
	MainWorldLoadingShownAtRealTime = -1.0;
}

void ANPMainPlayerController::ShowMainWorldLoadingFailure()
{
	ShowMainWorldLoadingOverlay();
	if (IsValid(MainWorldLoadingWidget))
	{
		MainWorldLoadingWidget->ShowFailure();
	}
}

void ANPMainPlayerController::PlayPhotoFlash()
{
	if (!IsLocalController())
	{
		return;
	}
	if (!PhotoFlashWidget)
	{
		UE_LOG(LogNPPhoto, Warning, TEXT("[PhotoUI] Flash skipped: PhotoFlashWidget is null."));
		return;
	}
	PhotoFlashWidget->PlayFlash();
	UE_LOG(LogNPPhoto, Log, TEXT("[PhotoUI] Flash animation requested."));
}

void ANPMainPlayerController::ClientPlayPhotographedFlash_Implementation()
{
	PlayPhotoFlash();
	UE_LOG(
		LogNPPhoto,
		Log,
		TEXT("[PhotoUI] Photographed flash received. Controller=%s"),
		*GetNameSafe(this));
}

void ANPMainPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (ChatComponent)
	{
		ChatComponent->BindChatInput(InputComponent);
	}

	if (IsLocalPlayerController())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInputComponent)
	{
		UE_LOG(LogNPPhoto, Error, TEXT("[Input] Photo bindings skipped: Enhanced Input Component is missing."));
		return;
	}

	if (AimAction)
	{
		EnhancedInputComponent->BindAction(
			AimAction,
			ETriggerEvent::Started,
			this,
			&ANPMainPlayerController::HandleAimStarted);
		EnhancedInputComponent->BindAction(
			AimAction,
			ETriggerEvent::Completed,
			this,
			&ANPMainPlayerController::HandleAimReleased);
		EnhancedInputComponent->BindAction(
			AimAction,
			ETriggerEvent::Canceled,
			this,
			&ANPMainPlayerController::HandleAimReleased);
		UE_LOG(
			LogNPPhoto,
			Log,
			TEXT("[Input] Shared aim action bound. Action=%s"),
			*GetNameSafe(AimAction));
	}
	else
	{
		UE_LOG(LogNPPhoto, Warning, TEXT("[Input] AimAction is not assigned."));
	}

	if (FireAction)
	{
		EnhancedInputComponent->BindAction(
			FireAction,
			ETriggerEvent::Started,
			this,
			&ANPMainPlayerController::HandleFireStarted);
		UE_LOG(
			LogNPPhoto,
			Log,
			TEXT("[Input] Shared fire action bound. Action=%s"),
			*GetNameSafe(FireAction));
	}
	else
	{
		UE_LOG(LogNPPhoto, Warning, TEXT("[Input] FireAction is not assigned."));
	}
}

bool ANPMainPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	if (IsLocalController() && Params.Key == EKeys::Escape && Params.Event == IE_Pressed)
	{
		ToggleOptionPanel();
		return true;
	}

	if (IsLocalController() && IsValid(ChatComponent) && ChatComponent->IsChatInputOpen()
		&& Params.Key == EKeys::LeftMouseButton && Params.Event == IE_Pressed)
	{
		ChatComponent->CloseChatInput();
		return true;
	}

	return Super::InputKey(Params);
}

void ANPMainPlayerController::ToggleOptionPanel()
{
	if (!IsLocalController() || !IsValid(OptionPanelWidgetClass))
	{
		return;
	}

	UNPUIManagerSubsystem* UIManager = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UNPUIManagerSubsystem>()
		: nullptr;
	if (!IsValid(UIManager))
	{
		return;
	}

	if (UNPUserWidget* TopWidget = UIManager->GetTopWidget();
		IsValid(TopWidget) && TopWidget->IsA(OptionPanelWidgetClass))
	{
		UIManager->RequestPopWidget();
		return;
	}

	if (UNPUserWidget* OptionPanel = UIManager->PushWidget(OptionPanelWidgetClass, 1000))
	{
		OptionPanel->SetInputModeState(ENPWidgetInputMode::GameAndUI);
		UIManager->RefreshTopWidgetInputMode();
	}
}

void ANPMainPlayerController::HandleAimStarted()
{
	UNPAbilitySystemComponent* AbilitySystem = ResolveAbilitySystem();
	if (!AbilitySystem)
	{
		UE_LOG(LogNPPhoto, Error, TEXT("[Input] AbilitySystemComponent is null."));
		return;
	}

	if (IsHoldingRelicWithAimView())
	{
		AbilitySystem->CancelPhotoAimAbility();
		AbilitySystem->ActivateRelicAimAbility();
		return;
	}

	if (bMainWorldInputLocked)
	{
		AbilitySystem->CancelPhotoAimAbility();
		return;
	}

	AbilitySystem->CancelRelicAimAbility();
	AbilitySystem->TogglePhotoAimAbility();
}

void ANPMainPlayerController::HandleAimReleased()
{
	if (UNPAbilitySystemComponent* AbilitySystem = ResolveAbilitySystem())
	{
		AbilitySystem->CancelRelicAimAbility();
	}
}

void ANPMainPlayerController::HandleFireStarted()
{
	UNPAbilitySystemComponent* AbilitySystem = ResolveAbilitySystem();
	if (!AbilitySystem)
	{
		UE_LOG(LogNPPhoto, Error, TEXT("[Input] AbilitySystemComponent is null."));
		return;
	}

	if (AbilitySystem->HasMatchingGameplayTag(
		NPGameplayTags::State_Relic_Aiming))
	{
		AbilitySystem->ActivateRelicFireAbility();
		return;
	}

	if (AbilitySystem->HasMatchingGameplayTag(
		NPGameplayTags::State_Photo_Aiming))
	{
		if (bMainWorldInputLocked)
		{
			AbilitySystem->CancelPhotoAimAbility();
			return;
		}

		AbilitySystem->ActivatePhotoShotAbility();
	}
}

void ANPMainPlayerController::BindAimCrosshairToAbilitySystem()
{
	if (!IsLocalController())
	{
		return;
	}

	UNPAbilitySystemComponent* AbilitySystem = ResolveAbilitySystem();
	if (!IsValid(AbilitySystem))
	{
		UnbindAimCrosshairFromAbilitySystem();
		SetAimCrosshairActive(false);
		SetPhotoAimWidgetActive(false);
		return;
	}

	if (AimCrosshairAbilitySystem.Get() != AbilitySystem)
	{
		UnbindAimCrosshairFromAbilitySystem();
		AimCrosshairAbilitySystem = AbilitySystem;
		RelicAimingTagChangedHandle = AbilitySystem->RegisterGameplayTagEvent(
			NPGameplayTags::State_Relic_Aiming,
			EGameplayTagEventType::NewOrRemoved).AddUObject(
				this,
				&ThisClass::HandleRelicAimingTagChanged);
		PhotoAimingTagChangedHandle = AbilitySystem->RegisterGameplayTagEvent(
			NPGameplayTags::State_Photo_Aiming,
			EGameplayTagEventType::NewOrRemoved).AddUObject(
				this,
				&ThisClass::HandlePhotoAimingTagChanged);
		PhotoCooldownTagChangedHandle = AbilitySystem->RegisterGameplayTagEvent(
			NPGameplayTags::Cooldown_Photo_Shot,
			EGameplayTagEventType::NewOrRemoved).AddUObject(
				this,
				&ThisClass::HandlePhotoCooldownTagChanged);
	}

	SetAimCrosshairActive(AbilitySystem->HasMatchingGameplayTag(
		NPGameplayTags::State_Relic_Aiming));
	SetPhotoAimWidgetActive(AbilitySystem->HasMatchingGameplayTag(
		NPGameplayTags::State_Photo_Aiming));
	HandlePhotoCooldownTagChanged(
		NPGameplayTags::Cooldown_Photo_Shot,
		AbilitySystem->GetTagCount(NPGameplayTags::Cooldown_Photo_Shot));
}

void ANPMainPlayerController::UnbindAimCrosshairFromAbilitySystem()
{
	if (UNPAbilitySystemComponent* AbilitySystem = AimCrosshairAbilitySystem.Get())
	{
		if (RelicAimingTagChangedHandle.IsValid())
		{
			AbilitySystem->RegisterGameplayTagEvent(
				NPGameplayTags::State_Relic_Aiming,
				EGameplayTagEventType::NewOrRemoved).Remove(
					RelicAimingTagChangedHandle);
		}
		if (PhotoAimingTagChangedHandle.IsValid())
		{
			AbilitySystem->RegisterGameplayTagEvent(
				NPGameplayTags::State_Photo_Aiming,
				EGameplayTagEventType::NewOrRemoved).Remove(
					PhotoAimingTagChangedHandle);
		}
		if (PhotoCooldownTagChangedHandle.IsValid())
		{
			AbilitySystem->RegisterGameplayTagEvent(
				NPGameplayTags::Cooldown_Photo_Shot,
				EGameplayTagEventType::NewOrRemoved).Remove(
					PhotoCooldownTagChangedHandle);
		}
	}

	RelicAimingTagChangedHandle.Reset();
	PhotoAimingTagChangedHandle.Reset();
	PhotoCooldownTagChangedHandle.Reset();
	StopPhotoCooldownDisplayUpdates(false);
	AimCrosshairAbilitySystem.Reset();
}

void ANPMainPlayerController::HandleRelicAimingTagChanged(
	const FGameplayTag Tag,
	const int32 NewCount)
{
	SetAimCrosshairActive(NewCount > 0);
}

void ANPMainPlayerController::HandlePhotoAimingTagChanged(
	const FGameplayTag Tag,
	const int32 NewCount)
{
	SetPhotoAimWidgetActive(NewCount > 0);
}

void ANPMainPlayerController::HandlePhotoCooldownTagChanged(
	const FGameplayTag Tag,
	const int32 NewCount)
{
	if (NewCount > 0)
	{
		BeginPhotoCooldownDisplayUpdates();
		return;
	}

	StopPhotoCooldownDisplayUpdates(true);
}

void ANPMainPlayerController::SetAimCrosshairActive(const bool bActive)
{
	if (IsLocalController() && IsValid(AimCrosshairWidget))
	{
		AimCrosshairWidget->SetAimActive(bActive);
	}
}

void ANPMainPlayerController::SetPhotoAimWidgetActive(const bool bActive)
{
	if (IsLocalController() && IsValid(PhotoAimWidget))
	{
		PhotoAimWidget->SetAimActive(bActive);
		UpdatePhotoCooldownDisplay();
	}
}

void ANPMainPlayerController::BeginPhotoCooldownDisplayUpdates()
{
	if (!IsLocalController())
	{
		return;
	}

	UpdatePhotoCooldownDisplay();
	GetWorldTimerManager().SetTimer(
		PhotoCooldownDisplayTimer,
		this,
		&ThisClass::UpdatePhotoCooldownDisplay,
		FMath::Max(0.02f, PhotoCooldownDisplayUpdateInterval),
		true);
}

void ANPMainPlayerController::StopPhotoCooldownDisplayUpdates(
	const bool bShowFullyCharged)
{
	GetWorldTimerManager().ClearTimer(PhotoCooldownDisplayTimer);
	if (bShowFullyCharged && IsValid(PhotoAimWidget))
	{
		PhotoAimWidget->SetCooldownDisplay(
			PhotoCooldownCellCount,
			PhotoCooldownCellCount,
			0.0f,
			0.0f);
	}
}

void ANPMainPlayerController::UpdatePhotoCooldownDisplay()
{
	if (!IsLocalController() || !IsValid(PhotoAimWidget))
	{
		return;
	}

	UNPAbilitySystemComponent* AbilitySystem =
		AimCrosshairAbilitySystem.Get();
	if (!IsValid(AbilitySystem)
		|| !AbilitySystem->HasMatchingGameplayTag(
			NPGameplayTags::Cooldown_Photo_Shot))
	{
		PhotoAimWidget->SetCooldownDisplay(
			PhotoCooldownCellCount,
			PhotoCooldownCellCount,
			0.0f,
			0.0f);
		return;
	}

	FGameplayTagContainer CooldownTags;
	CooldownTags.AddTag(NPGameplayTags::Cooldown_Photo_Shot);
	const FGameplayEffectQuery Query =
		FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(CooldownTags);
	const TArray<TPair<float, float>> Times =
		AbilitySystem->GetActiveEffectsTimeRemainingAndDuration(Query);

	float RemainingTime = 0.0f;
	float Duration = 0.0f;
	for (const TPair<float, float>& Time : Times)
	{
		if (Time.Key > RemainingTime)
		{
			RemainingTime = Time.Key;
			Duration = Time.Value;
		}
	}

	if (Duration <= 0.0f)
	{
		PhotoAimWidget->SetCooldownDisplay(
			0,
			PhotoCooldownCellCount,
			RemainingTime,
			Duration);
		return;
	}

	const float ChargedRatio = FMath::Clamp(
		1.0f - RemainingTime / Duration,
		0.0f,
		1.0f);
	const int32 ChargedCellCount = FMath::Clamp(
		FMath::FloorToInt(
			ChargedRatio * static_cast<float>(PhotoCooldownCellCount)
			+ KINDA_SMALL_NUMBER),
		0,
		PhotoCooldownCellCount);
	PhotoAimWidget->SetCooldownDisplay(
		ChargedCellCount,
		PhotoCooldownCellCount,
		RemainingTime,
		Duration);
}

bool ANPMainPlayerController::IsHoldingRelicWithAimView() const
{
	const APawn* ControlledPawn = GetPawn();
	const UNPStablePhysicsGrabComponent* GrabComponent = ControlledPawn
		? ControlledPawn->FindComponentByClass<UNPStablePhysicsGrabComponent>()
		: nullptr;
	const UPrimitiveComponent* GrabbedComponent = GrabComponent
		? GrabComponent->GetGrabbedComponent()
		: nullptr;
	const AActor* GrabbedActor = GrabbedComponent
		? GrabbedComponent->GetOwner()
		: nullptr;
	return GrabbedActor
		&& (GrabbedActor->FindComponentByClass<UNPAimableRelicComponent>()
			|| GrabbedActor->FindComponentByClass<UNPThrowableRelicComponent>());
}

UNPAbilitySystemComponent*
ANPMainPlayerController::ResolveAbilitySystem() const
{
	const ANPReplicatedStablePhysicsPawn* StablePawn =
		Cast<ANPReplicatedStablePhysicsPawn>(GetPawn());
	return StablePawn
		? Cast<UNPAbilitySystemComponent>(
			StablePawn->GetAbilitySystemComponent())
		: nullptr;
}

bool ANPMainPlayerController::ShouldUseTouchControls() const
{
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

bool ANPMainPlayerController::IsListenServerHost() const
{
	return IsLocalController() && HasAuthority();
}

void ANPMainPlayerController::RequestRestartRoom()
{
	if (IsLocalController())
	{
		ServerRequestRestartRoom();
	}
}

void ANPMainPlayerController::ServerRequestRestartRoom_Implementation()
{
	if (ANPMainGameMode* MainGameMode = GetWorld()
		? GetWorld()->GetAuthGameMode<ANPMainGameMode>()
		: nullptr)
	{
		MainGameMode->RequestRestartRoom(this);
	}
}

void ANPMainPlayerController::ExitToMainMenu()
{
	if (!IsLocalController() || MainMenuLevel.IsNull())
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UNPRoomSubsystem* RoomSubsystem = GameInstance
		? GameInstance->GetSubsystem<UNPRoomSubsystem>()
		: nullptr;

	if (!RoomSubsystem)
	{
		return;
	}

	const FString MenuLevelPath = MainMenuLevel.ToSoftObjectPath().GetLongPackageName();
	RoomSubsystem->LeaveRoom(MenuLevelPath);
}

void ANPMainPlayerController::ShowGameScreenUI()
{
	ShowSingleScreen(GameScreenWidgetClass);
	EnsureNoticeEventWidget();
}

void ANPMainPlayerController::ClientShowGameScreenUI_Implementation()
{
	ShowGameScreenUI();
}

void ANPMainPlayerController::ShowSelectPictureUI()
{
	ShowSingleScreen(SelectPictureWidgetClass);
}

void ANPMainPlayerController::ClientShowSelectPictureUI_Implementation()
{
	ShowSelectPictureUI();
}

void ANPMainPlayerController::ShowResultUI()
{
	ShowSingleScreen(ResultWidgetClass);
}

void ANPMainPlayerController::ClientShowResultUI_Implementation()
{
	ShowResultUI();
}

void ANPMainPlayerController::ServerConfirmPictureSelection_Implementation(
	const TArray<FGuid>& SelectedPhotoIds)
{
	ANPMainGameState* MainGameState = GetWorld()
		? GetWorld()->GetGameState<ANPMainGameState>()
		: nullptr;

	if (!IsValid(MainGameState) || !IsValid(PlayerState)
		|| SelectedPhotoIds.Num() > 5 || !PendingSelectedPhotoIds.IsEmpty())
	{
		return;
	}

	//선택된 사진들이 이 플레이어가 찍은 성공 사진인지 검증
	TSet<FGuid> VerifiedPhotoIds;
	TMap<FGuid, uint16> CaptureSequences;

	for (const FGuid& PhotoId : SelectedPhotoIds)
	{
		if (!PhotoId.IsValid()
			|| VerifiedPhotoIds.Contains(PhotoId))
		{
			return;
		}

		bool bIsOwnedSuccessPhoto = false;

		for (const FNPReplicatedPhotoEvidence& Evidence :
			MainGameState->GetPhotoEvidence())
		{
			if (Evidence.PhotoId == PhotoId
				&& Evidence.Photographer == PlayerState)
			{
				bIsOwnedSuccessPhoto = true;
				CaptureSequences.Add(PhotoId, static_cast<uint16>(Evidence.CaptureSequence));
				break;
			}
		}

		if (!bIsOwnedSuccessPhoto)
		{
			return;
		}

		VerifiedPhotoIds.Add(PhotoId);
	}

	MainGameState->SetSelectedPhotoIds(PlayerState, SelectedPhotoIds);
	if (SelectedPhotoIds.IsEmpty())
	{
		MainGameState->ConfirmPictureSelection(this);
		return;
	}

	ANPMainGameMode* MainGameMode = GetWorld()
		? GetWorld()->GetAuthGameMode<ANPMainGameMode>()
		: nullptr;
	UNPPhotoRepository* Repository = IsValid(MainGameMode)
		? MainGameMode->GetPhotoRepository()
		: nullptr;
	if (!IsValid(Repository))
	{
		return;
	}

	PendingSelectedPhotoIds = VerifiedPhotoIds;
	for (const FGuid& PhotoId : SelectedPhotoIds)
	{
		Repository->AuthorizeCapture(this, PhotoId, CaptureSequences[PhotoId]);
	}
	ClientUploadSelectedPhotos(SelectedPhotoIds);
}

void ANPMainPlayerController::ServerLikeResultPhoto_Implementation(const FGuid PhotoId)
{
	ANPMainGameState* MainGameState = GetWorld()
		? GetWorld()->GetGameState<ANPMainGameState>()
		: nullptr;
	if (IsValid(MainGameState) && IsValid(PlayerState))
	{
		MainGameState->AddPhotoLike(PhotoId, PlayerState);
	}
}

void ANPMainPlayerController::ClientUploadSelectedPhotos_Implementation(
	const TArray<FGuid>& SelectedPhotoIds)
{
	if (!IsValid(PhotoCaptureComponent) || !IsValid(PhotoTransferComponent))
	{
		for (const FGuid& PhotoId : SelectedPhotoIds)
		{
			ServerReportSelectedPhotoUploadFailed(PhotoId);
		}
		return;
	}

	for (const FGuid& PhotoId : SelectedPhotoIds)
	{
		uint16 CaptureSequence = 0;
		const TArray<uint8>* JpegData = nullptr;
		int32 Width = 0;
		int32 Height = 0;
		if (!PhotoCaptureComponent->GetLocalPhotoData(
				PhotoId, CaptureSequence, JpegData, Width, Height)
			|| !JpegData
			|| !PhotoTransferComponent->BeginUploadPhoto(
				PhotoId, CaptureSequence, *JpegData, Width, Height))
		{
			ServerReportSelectedPhotoUploadFailed(PhotoId);
		}
	}
}

void ANPMainPlayerController::ServerReportSelectedPhotoUploadFailed_Implementation(
	const FGuid PhotoId)
{
	if (!PendingSelectedPhotoIds.Remove(PhotoId))
	{
		return;
	}

	if (ANPMainGameState* MainGameState = GetWorld()
		? GetWorld()->GetGameState<ANPMainGameState>()
		: nullptr)
	{
		TArray<FGuid> StoredSelection = MainGameState->GetSelectedPhotoIds(PlayerState);
		StoredSelection.Remove(PhotoId);
		MainGameState->SetSelectedPhotoIds(PlayerState, StoredSelection);
	}
	TryCompletePictureSelection();
}

void ANPMainPlayerController::HandleSelectedPhotoStored(const FGuid& PhotoId)
{
	if (HasAuthority() && PendingSelectedPhotoIds.Remove(PhotoId))
	{
		TryCompletePictureSelection();
	}
}

void ANPMainPlayerController::TryCompletePictureSelection()
{
	if (!HasAuthority() || !PendingSelectedPhotoIds.IsEmpty())
	{
		return;
	}

	if (ANPMainGameState* MainGameState = GetWorld()
		? GetWorld()->GetGameState<ANPMainGameState>()
		: nullptr)
	{
		MainGameState->ConfirmPictureSelection(this);
	}
}

void ANPMainPlayerController::ShowSingleScreen(
	TSubclassOf<UNPUserWidget> WidgetClass)
{
	if (!IsLocalController() || !IsValid(WidgetClass))
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();

	UNPUIManagerSubsystem* UIManager = GameInstance
		? GameInstance->GetSubsystem<UNPUIManagerSubsystem>()
		: nullptr;

	if (!UIManager)
	{
		return;
	}

	UIManager->PopAllWidgets();
	UIManager->PushWidget(WidgetClass);
}

void ANPMainPlayerController::EnsureNoticeEventWidget()
{
	if (!IsLocalController() || IsValid(NoticeEventWidget))
	{
		return;
	}

	if (!IsValid(NoticeEventWidgetClass))
	{
		return;
	}

	NoticeEventWidget = CreateWidget<UNPNoticeEventWidget>(this, NoticeEventWidgetClass);
	if (!IsValid(NoticeEventWidget))
	{
		return;
	}

	NoticeEventWidget->AddToPlayerScreen(50);
}
