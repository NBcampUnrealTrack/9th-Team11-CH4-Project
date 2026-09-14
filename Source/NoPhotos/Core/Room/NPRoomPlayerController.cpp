#include "Core/Room/NPRoomPlayerController.h"

#include "AbilitySystemComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "Core/Chat/NPChatComponent.h"
#include "Core/Component/NPRoomPlayerComponent.h"
#include "Core/GameplayTag/NPGameplayTags.h"
#include "Core/Room/NPRoomCheatManager.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Gameplay/AbilitySystem/NPAbilitySystemComponent.h"
#include "Gameplay/Character/NPReplicatedStablePhysicsPawn.h"
#include "Gameplay/Photo/NPPhotoCaptureComponent.h"
#include "Gameplay/Photo/NPPhotoFlashWidget.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "SubSystem/NPUIManagerSubsystem.h"
#include "UI/GameScreen/NPAimCrosshairWidget.h"
#include "UI/NPUserWidget.h"
#include "UObject/ConstructorHelpers.h"
#include "UI/GameScreen/Relic/NPRelicUsePromptUIComponent.h"
#include "Widgets/Input/SVirtualJoystick.h"

ANPRoomPlayerController::ANPRoomPlayerController()
	: ChangeInputAction(nullptr)
{
	RoomComponent = CreateDefaultSubobject<UNPRoomPlayerComponent>(TEXT("RoomComponent"));
	ChatComponent = CreateDefaultSubobject<UNPChatComponent>(TEXT("ChatComponent"));
	PhotoCaptureComponent = CreateDefaultSubobject<UNPPhotoCaptureComponent>(
		TEXT("PhotoCaptureComponent"));
	PhotoCaptureComponent->SetPresentationOnly(true);
	RelicUsePromptUIComponent = CreateDefaultSubobject<UNPRelicUsePromptUIComponent>(
		TEXT("RelicUsePromptUIComponent"));
	CheatClass = UNPRoomCheatManager::StaticClass();

	static ConstructorHelpers::FObjectFinder<UInputAction> AimInputAction(
		TEXT("/Game/Input/Actions/IA_Aim.IA_Aim"));
	static ConstructorHelpers::FObjectFinder<UInputAction> FireInputAction(
		TEXT("/Game/Input/Actions/IA_Fire.IA_Fire"));
	static ConstructorHelpers::FClassFinder<UNPAimCrosshairWidget>
		PhotoAimWidgetFinder(
			TEXT("/Game/NoPhotos/Blueprints/UI/GameScreen/Aim/WBP_NPPhotoAim"));
	static ConstructorHelpers::FClassFinder<UNPPhotoFlashWidget>
		PhotoFlashWidgetFinder(
			TEXT("/Game/NoPhotos/tempUI/WBP_NPPhotoFlashWidget"));
	if (AimInputAction.Succeeded())
	{
		AimAction = AimInputAction.Object;
	}
	if (FireInputAction.Succeeded())
	{
		FireAction = FireInputAction.Object;
	}
	if (PhotoAimWidgetFinder.Succeeded())
	{
		PhotoAimWidgetClass = PhotoAimWidgetFinder.Class;
	}
	if (PhotoFlashWidgetFinder.Succeeded())
	{
		PhotoFlashWidgetClass = PhotoFlashWidgetFinder.Class;
	}
}

void ANPRoomPlayerController::PlayPhotoFlash()
{
	if (IsLocalController() && IsValid(PhotoFlashWidget))
	{
		PhotoFlashWidget->PlayFlash();
	}
}

void ANPRoomPlayerController::PlayPresentationOnlyShutterCue()
{
	if (!HasAuthority())
	{
		return;
	}

	APawn* ControlledPawn = GetPawn();
	UNPAbilitySystemComponent* AbilitySystem = ResolveAbilitySystem();
	if (!IsValid(ControlledPawn) || !IsValid(AbilitySystem))
	{
		return;
	}

	FGameplayCueParameters Parameters;
	Parameters.Location = ControlledPawn->GetActorLocation();
	Parameters.Normal = ControlledPawn->GetActorForwardVector();
	Parameters.Instigator = ControlledPawn;
	Parameters.EffectCauser = ControlledPawn;
	AbilitySystem->ExecuteGameplayCue(
		NPGameplayTags::GameplayCue_Photo_Shutter,
		Parameters);
	AbilitySystem->ExecuteGameplayCue(
		NPGameplayTags::GameplayCue_Photo_WorldFeedback_Photographer,
		Parameters);
}

void ANPRoomPlayerController::RequestStartGame()
{
	if (RoomComponent)
	{
		RoomComponent->RequestStartGame();
	}
}

void ANPRoomPlayerController::RequestRestartRoom()
{
	if (RoomComponent)
	{
		RoomComponent->RequestRestartRoom();
	}
}

void ANPRoomPlayerController::ExitRoom()
{
	if (RoomComponent)
	{
		RoomComponent->ExitRoom();
	}
}

void ANPRoomPlayerController::ShowRoomUsers() const
{
	if (RoomComponent)
	{
		RoomComponent->ShowRoomUsers();
	}
}

bool ANPRoomPlayerController::IsRoomHost() const
{
	return RoomComponent && RoomComponent->IsRoomHost();
}

bool ANPRoomPlayerController::CanStartGame() const
{
	return RoomComponent && RoomComponent->CanStartGame();
}

void ANPRoomPlayerController::ShowLobbyUI()
{
	ShowSingleScreen(LobbyWidgetClass);
	bIsMouseInput = false;
	SetCharacterInputMappingEnabled(true);
	SetLobbyInputMappingEnabled(true);
	ApplyLobbyInputMode();
}

void ANPRoomPlayerController::ClientShowLobbyUI_Implementation()
{
	ShowLobbyUI();
}

void ANPRoomPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
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

		if (PhotoFlashWidgetClass)
		{
			PhotoFlashWidget = CreateWidget<UNPPhotoFlashWidget>(
				this,
				PhotoFlashWidgetClass);
			if (IsValid(PhotoFlashWidget))
			{
				PhotoFlashWidget->AddToPlayerScreen(100);
				PhotoFlashWidget->SetVisibility(ESlateVisibility::Collapsed);
			}
		}

		BindPhotoUIToAbilitySystem();
	}

	if (!ShouldUseTouchControls() || !IsLocalPlayerController() || !MobileControlsWidgetClass)
	{
		return;
	}

	MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);
	if (MobileControlsWidget)
	{
		MobileControlsWidget->AddToPlayerScreen(0);
	}
}

void ANPRoomPlayerController::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (UNPAbilitySystemComponent* AbilitySystem = ResolveAbilitySystem())
	{
		AbilitySystem->CancelPhotoAimAbility();
	}
	UnbindPhotoUIFromAbilitySystem();
	Super::EndPlay(EndPlayReason);
}

void ANPRoomPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	BindPhotoUIToAbilitySystem();
}

void ANPRoomPlayerController::OnRep_Pawn()
{
	Super::OnRep_Pawn();
	BindPhotoUIToAbilitySystem();
}

void ANPRoomPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!IsLocalPlayerController())
	{
		return;
	}

	SetCharacterInputMappingEnabled(true);
	if (ChatComponent)
	{
		ChatComponent->BindChatInput(InputComponent);
	}

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (ChangeInputAction)
		{
			EnhancedInput->BindAction(ChangeInputAction, ETriggerEvent::Started, this, &ANPRoomPlayerController::ToggleLobbyInputMode);
		}
		if (AimAction)
		{
			EnhancedInput->BindAction(
				AimAction,
				ETriggerEvent::Started,
				this,
				&ThisClass::HandlePhotoAimStarted);
		}
		if (FireAction)
		{
			EnhancedInput->BindAction(
				FireAction,
				ETriggerEvent::Started,
				this,
				&ThisClass::HandlePhotoFireStarted);
		}
	}
}

void ANPRoomPlayerController::HandlePhotoAimStarted()
{
	if (bIsMouseInput
		|| (IsValid(ChatComponent) && ChatComponent->IsChatInputOpen()))
	{
		return;
	}

	if (UNPAbilitySystemComponent* AbilitySystem = ResolveAbilitySystem())
	{
		AbilitySystem->TogglePhotoAimAbility();
	}
}

void ANPRoomPlayerController::HandlePhotoFireStarted()
{
	if (bIsMouseInput
		|| (IsValid(ChatComponent) && ChatComponent->IsChatInputOpen()))
	{
		return;
	}

	UNPAbilitySystemComponent* AbilitySystem = ResolveAbilitySystem();
	if (IsValid(AbilitySystem)
		&& AbilitySystem->HasMatchingGameplayTag(
			NPGameplayTags::State_Photo_Aiming))
	{
		AbilitySystem->ActivatePhotoShotAbility();
	}
}

bool ANPRoomPlayerController::InputKey(const FInputKeyEventArgs& Params)
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

	const bool bHandled = Super::InputKey(Params);
	if (IsLocalController() && bIsMouseInput
		&& Params.Key == EKeys::LeftMouseButton && Params.Event == IE_Pressed
		&& (!IsValid(ChatComponent) || !ChatComponent->IsChatInputOpen()))
	{
		ApplyLobbyInputMode();
	}

	return bHandled;
}

void ANPRoomPlayerController::ToggleOptionPanel()
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

bool ANPRoomPlayerController::ShouldUseTouchControls() const
{
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

void ANPRoomPlayerController::ToggleLobbyInputMode()
{
	if (!IsLocalController())
	{
		return;
	}

	bIsMouseInput = !bIsMouseInput;
	if (bIsMouseInput)
	{
		if (UNPAbilitySystemComponent* AbilitySystem = ResolveAbilitySystem())
		{
			AbilitySystem->CancelPhotoAimAbility();
		}
	}
	SetCharacterInputMappingEnabled(!bIsMouseInput);
	ApplyLobbyInputMode();
}

void ANPRoomPlayerController::BindPhotoUIToAbilitySystem()
{
	if (!IsLocalController())
	{
		return;
	}

	UNPAbilitySystemComponent* AbilitySystem = ResolveAbilitySystem();
	if (!IsValid(AbilitySystem))
	{
		UnbindPhotoUIFromAbilitySystem();
		SetPhotoAimWidgetActive(false);
		return;
	}

	if (PhotoUIAbilitySystem.Get() != AbilitySystem)
	{
		UnbindPhotoUIFromAbilitySystem();
		PhotoUIAbilitySystem = AbilitySystem;
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

	SetPhotoAimWidgetActive(AbilitySystem->HasMatchingGameplayTag(
		NPGameplayTags::State_Photo_Aiming));
	HandlePhotoCooldownTagChanged(
		NPGameplayTags::Cooldown_Photo_Shot,
		AbilitySystem->GetTagCount(NPGameplayTags::Cooldown_Photo_Shot));
}

void ANPRoomPlayerController::UnbindPhotoUIFromAbilitySystem()
{
	if (UNPAbilitySystemComponent* AbilitySystem = PhotoUIAbilitySystem.Get())
	{
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

	PhotoAimingTagChangedHandle.Reset();
	PhotoCooldownTagChangedHandle.Reset();
	StopPhotoCooldownDisplayUpdates(false);
	PhotoUIAbilitySystem.Reset();
}

void ANPRoomPlayerController::HandlePhotoAimingTagChanged(
	const FGameplayTag Tag,
	const int32 NewCount)
{
	SetPhotoAimWidgetActive(NewCount > 0);
}

void ANPRoomPlayerController::HandlePhotoCooldownTagChanged(
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

void ANPRoomPlayerController::SetPhotoAimWidgetActive(const bool bActive)
{
	if (IsLocalController() && IsValid(PhotoAimWidget))
	{
		PhotoAimWidget->SetAimActive(bActive);
		UpdatePhotoCooldownDisplay();
	}
}

void ANPRoomPlayerController::BeginPhotoCooldownDisplayUpdates()
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

void ANPRoomPlayerController::StopPhotoCooldownDisplayUpdates(
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

void ANPRoomPlayerController::UpdatePhotoCooldownDisplay()
{
	if (!IsLocalController() || !IsValid(PhotoAimWidget))
	{
		return;
	}

	UNPAbilitySystemComponent* AbilitySystem = PhotoUIAbilitySystem.Get();
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

UNPAbilitySystemComponent* ANPRoomPlayerController::ResolveAbilitySystem() const
{
	const ANPReplicatedStablePhysicsPawn* StablePawn =
		Cast<ANPReplicatedStablePhysicsPawn>(GetPawn());
	return StablePawn
		? Cast<UNPAbilitySystemComponent>(
			StablePawn->GetAbilitySystemComponent())
		: nullptr;
}

void ANPRoomPlayerController::SetCharacterInputMappingEnabled(const bool bEnabled)
{
	if (!IsLocalController())
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (!IsValid(InputSubsystem))
	{
		return;
	}

	for (UInputMappingContext* MappingContext : DefaultMappingContexts)
	{
		if (IsValid(MappingContext) && MappingContext != InputMappingContext)
		{
			if (bEnabled)
			{
				InputSubsystem->AddMappingContext(MappingContext, 0);
			}
			else
			{
				InputSubsystem->RemoveMappingContext(MappingContext);
			}
		}
	}

	if (ShouldUseTouchControls())
	{
		return;
	}

	for (UInputMappingContext* MappingContext : MobileExcludedMappingContexts)
	{
		if (IsValid(MappingContext) && MappingContext != InputMappingContext)
		{
			if (bEnabled)
			{
				InputSubsystem->AddMappingContext(MappingContext, 0);
			}
			else
			{
				InputSubsystem->RemoveMappingContext(MappingContext);
			}
		}
	}
}

// 로비 진입이나 퇴장에 맞춰 입력모드 변경
void ANPRoomPlayerController::SetLobbyInputMappingEnabled(const bool bEnabled)
{
	if (!IsLocalController() || !IsValid(InputMappingContext))
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (!IsValid(InputSubsystem))
	{
		return;
	}

	//로비라면
	if (bEnabled)
	{
		InputSubsystem->AddMappingContext(InputMappingContext, 0);
		return;
	}

	InputSubsystem->RemoveMappingContext(InputMappingContext);
}

//bIsMouseInput 값을 입력 모드에 적용
void ANPRoomPlayerController::ApplyLobbyInputMode()
{
	if (!IsLocalController())
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UNPUIManagerSubsystem* UIManager = GameInstance	? GameInstance->GetSubsystem<UNPUIManagerSubsystem>() : nullptr;
	UNPUserWidget* TopWidget = UIManager ? UIManager->GetTopWidget() : nullptr;
	if (!IsValid(TopWidget))
	{
		return;
	}

	TopWidget->SetInputModeState(bIsMouseInput ? ENPWidgetInputMode::GameAndUI : ENPWidgetInputMode::GameOnly);
	UIManager->RefreshTopWidgetInputMode();
}

void ANPRoomPlayerController::ShowSingleScreen(TSubclassOf<UNPUserWidget> WidgetClass)
{
	if (!IsLocalController() || !IsValid(WidgetClass))
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UNPUIManagerSubsystem* UIManager = GameInstance	? GameInstance->GetSubsystem<UNPUIManagerSubsystem>() : nullptr;
	if (!UIManager)
	{
		return;
	}

	UIManager->PopAllWidgets();
	UIManager->PushWidget(WidgetClass);
}
