// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/Room/NPRoomPlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "Core/Chat/NPChatComponent.h"
#include "Core/Component/NPRoomPlayerComponent.h"
#include "Core/Room/NPRoomCheatManager.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "SubSystem/NPUIManagerSubsystem.h"
#include "UI/NPUserWidget.h"
#include "Widgets/Input/SVirtualJoystick.h"

ANPRoomPlayerController::ANPRoomPlayerController()
	: ChangeInputAction(nullptr)
{
	RoomComponent = CreateDefaultSubobject<UNPRoomPlayerComponent>(TEXT("RoomComponent"));
	ChatComponent = CreateDefaultSubobject<UNPChatComponent>(TEXT("ChatComponent"));
	CheatClass = UNPRoomCheatManager::StaticClass();
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
			EnhancedInput->BindAction(ChangeInputAction, ETriggerEvent::Started, this, &ANPRoomPlayerController::ChangeInputMode);
		}
	}
}

bool ANPRoomPlayerController::ShouldUseTouchControls() const
{
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

void ANPRoomPlayerController::ChangeInputMode()
{
	if (!IsLocalController())
	{
		return;
	}

	bIsMouseInput = !bIsMouseInput;
	SetCharacterInputMappingEnabled(!bIsMouseInput);
	ApplyLobbyInputMode();
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
