#include "Core/Main/NPMainPlayerController.h"

#include "Core/Main/NPMainGameMode.h"
#include "Core/Main/NPMainGameState.h"
#include "Core/Chat/NPChatComponent.h"
#include "Core/Room/NPRoomSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Components/PrimitiveComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "InputCoreTypes.h"
#include "Gameplay/Photo/NPPhotoCaptureComponent.h"
#include "Gameplay/Photo/NPPhotoFlashWidget.h"
#include "Gameplay/Photo/NPPhotoLog.h"
#include "Gameplay/Photo/NPPhotoTransferComponent.h"
#include "Gameplay/AbilitySystem/NPAbilitySystemComponent.h"
#include "Gameplay/Character/Component/NPStablePhysicsGrabComponent.h"
#include "Gameplay/Character/NPReplicatedStablePhysicsPawn.h"
#include "Gameplay/Relic/Components/NPAimableRelicComponent.h"
#include "NoPhotos.h"
#include "SubSystem/NPUIManagerSubsystem.h"
#include "SubSystem/Room/NPRoomGenerateSubsystem.h"
#include "UI/GameScreen/Event/NPNoticeEventWidget.h"
#include "UI/Loading/NPMainWorldLoadingWidget.h"
#include "UI/NPUserWidget.h"
#include "UObject/ConstructorHelpers.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "InputAction.h"
#include "InputMappingContext.h"

ANPMainPlayerController::ANPMainPlayerController()
{
	PhotoCaptureComponent = CreateDefaultSubobject<UNPPhotoCaptureComponent>(TEXT("PhotoCaptureComponent"));
	PhotoTransferComponent = CreateDefaultSubobject<UNPPhotoTransferComponent>(TEXT("PhotoTransferComponent"));
	ChatComponent = CreateDefaultSubobject<UNPChatComponent>(TEXT("ChatComponent"));

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
		SetMainWorldInputLocked(true);
		ShowMainWorldLoadingOverlay();
		BindRoomGenerationState();

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

void ANPMainPlayerController::ClientBeginMainWorldPreparation_Implementation()
{
	if (!IsLocalController())
	{
		return;
	}

	bReportedMainWorldReady = false;
	SetMainWorldInputLocked(true);
	ShowMainWorldLoadingOverlay();
	BindRoomGenerationState();
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

	bReportedMainWorldReady = true;
	ServerReportMainWorldReady();
}

void ANPMainPlayerController::HandleLocalRoomGenerationFailed()
{
	if (IsLocalController())
	{
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
	SetIgnoreMoveInput(bLocked);
	SetIgnoreLookInput(bLocked);
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
		}
	}

	if (IsValid(MainWorldLoadingWidget))
	{
		MainWorldLoadingWidget->ShowLoading();
	}
}

void ANPMainPlayerController::HideMainWorldLoadingOverlay()
{
	if (!IsValid(MainWorldLoadingWidget))
	{
		return;
	}

	MainWorldLoadingWidget->RemoveFromParent();
	MainWorldLoadingWidget = nullptr;
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
	if (IsLocalController() && IsValid(ChatComponent) && ChatComponent->IsChatInputOpen()
		&& Params.Key == EKeys::LeftMouseButton && Params.Event == IE_Pressed)
	{
		ChatComponent->CloseChatInput();
		return true;
	}

	return Super::InputKey(Params);
}

void ANPMainPlayerController::HandleAimStarted()
{
	if (!PhotoCaptureComponent)
	{
		UE_LOG(LogNPPhoto, Error, TEXT("[Input] PhotoCaptureComponent is null."));
		return;
	}

	if (IsHoldingAimableRelic())
	{
		if (PhotoCaptureComponent->IsPhotoModeActive())
		{
			PhotoCaptureComponent->ExitPhotoMode();
		}
		if (UNPAbilitySystemComponent* AbilitySystem =
			ResolveRelicAbilitySystem())
		{
			AbilitySystem->ActivateRelicAimAbility();
		}
		return;
	}

	PhotoCaptureComponent->TogglePhotoMode();
	UE_LOG(LogNPPhoto, Log, TEXT("[Input] Photo mode toggled. Active=%s"),
		PhotoCaptureComponent->IsPhotoModeActive() ? TEXT("true") : TEXT("false"));
}

void ANPMainPlayerController::HandleAimReleased()
{
	if (UNPAbilitySystemComponent* AbilitySystem =
		ResolveRelicAbilitySystem())
	{
		AbilitySystem->CancelRelicAimAbility();
	}
}

void ANPMainPlayerController::HandleFireStarted()
{
	if (!PhotoCaptureComponent)
	{
		UE_LOG(LogNPPhoto, Error, TEXT("[Input] PhotoCaptureComponent is null."));
		return;
	}

	if (!PhotoCaptureComponent->IsPhotoModeActive())
	{
		return;
	}

	UE_LOG(LogNPPhoto, Log, TEXT("[Input] Photo input received. Controller=%s Local=%s"),
		*GetNameSafe(this), IsLocalController() ? TEXT("true") : TEXT("false"));
	const bool bStarted = PhotoCaptureComponent->TakePhoto();
	UE_LOG(LogNPPhoto, Log, TEXT("[Input] TakePhoto result=%s"), bStarted ? TEXT("success") : TEXT("failed"));
}

bool ANPMainPlayerController::IsHoldingAimableRelic() const
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
		&& GrabbedActor->FindComponentByClass<UNPAimableRelicComponent>();
}

UNPAbilitySystemComponent*
ANPMainPlayerController::ResolveRelicAbilitySystem() const
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

	if (!IsValid(MainGameState) || !IsValid(PlayerState))
	{
		return;
	}

	//선택된 사진들이 이 플레이어가 찍은 성공 사진인지 검증
	TSet<FGuid> VerifiedPhotoIds;

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
	MainGameState->ConfirmPictureSelection(this);
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
