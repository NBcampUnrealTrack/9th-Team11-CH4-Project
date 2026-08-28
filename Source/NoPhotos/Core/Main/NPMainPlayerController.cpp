#include "Core/Main/NPMainPlayerController.h"

#include "Core/Main/NPMainGameMode.h"
#include "Core/Main/NPMainGameState.h"
#include "Core/Room/NPRoomSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Gameplay/Photo/NPPhotoCaptureComponent.h"
#include "Gameplay/Photo/NPPhotoFlashWidget.h"
#include "Gameplay/Photo/NPPhotoLog.h"
#include "Gameplay/Photo/NPPhotoTransferComponent.h"
#include "NoPhotos.h"
#include "SubSystem/NPUIManagerSubsystem.h"
#include "UI/GameScreen/Event/NPNoticeEventWidget.h"
#include "UI/NPUserWidget.h"
#include "UObject/ConstructorHelpers.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "UObject/ConstructorHelpers.h"

ANPMainPlayerController::ANPMainPlayerController()
{
	PhotoCaptureComponent = CreateDefaultSubobject<UNPPhotoCaptureComponent>(TEXT("PhotoCaptureComponent"));
	PhotoTransferComponent = CreateDefaultSubobject<UNPPhotoTransferComponent>(TEXT("PhotoTransferComponent"));

	static ConstructorHelpers::FObjectFinder<UInputMappingContext>
	DefaultMapping(TEXT("/Game/Input/IMC_Default.IMC_Default"));

	static ConstructorHelpers::FObjectFinder<UInputMappingContext>
		MouseLookMapping(TEXT("/Game/Input/IMC_MouseLook.IMC_MouseLook"));

	static ConstructorHelpers::FObjectFinder<UInputAction>
		PhotoModeAction(TEXT("/Game/Input/Actions/IA_PhotoMode.IA_PhotoMode"));

	static ConstructorHelpers::FObjectFinder<UInputAction>
		PhotoShotAction(TEXT("/Game/Input/Actions/IA_PhotoShot.IA_PhotoShot"));

	if (DefaultMapping.Succeeded())
	{
		DefaultMappingContexts.Add(DefaultMapping.Object);
	}

	if (MouseLookMapping.Succeeded())
	{
		DefaultMappingContexts.Add(MouseLookMapping.Object);
	}

	if (PhotoModeAction.Succeeded())
	{
		TogglePhotoModeAction = PhotoModeAction.Object;
	}

	if (PhotoShotAction.Succeeded())
	{
		TakePhotoAction = PhotoShotAction.Object;
	}
}

void ANPMainPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
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

	if (TogglePhotoModeAction)
	{
		EnhancedInputComponent->BindAction(TogglePhotoModeAction, ETriggerEvent::Started,
			this, &ANPMainPlayerController::HandleTogglePhotoModeInput);
		UE_LOG(LogNPPhoto, Log, TEXT("[Input] Photo mode action bound. Action=%s"), *GetNameSafe(TogglePhotoModeAction));
	}
	else
	{
		UE_LOG(LogNPPhoto, Warning, TEXT("[Input] TogglePhotoModeAction is not assigned."));
	}

	if (TakePhotoAction)
	{
		EnhancedInputComponent->BindAction(TakePhotoAction, ETriggerEvent::Started,
			this, &ANPMainPlayerController::HandleTakePhotoInput);
		UE_LOG(LogNPPhoto, Log, TEXT("[Input] Photo shot action bound. Action=%s"), *GetNameSafe(TakePhotoAction));
	}
	else
	{
		UE_LOG(LogNPPhoto, Warning, TEXT("[Input] TakePhotoAction is not assigned."));
	}
}

void ANPMainPlayerController::HandleTogglePhotoModeInput()
{
	if (!PhotoCaptureComponent)
	{
		UE_LOG(LogNPPhoto, Error, TEXT("[Input] PhotoCaptureComponent is null."));
		return;
	}
	PhotoCaptureComponent->TogglePhotoMode();
	UE_LOG(LogNPPhoto, Log, TEXT("[Input] Photo mode toggled. Active=%s"),
		PhotoCaptureComponent->IsPhotoModeActive() ? TEXT("true") : TEXT("false"));
}

void ANPMainPlayerController::HandleTakePhotoInput()
{
	UE_LOG(LogNPPhoto, Log, TEXT("[Input] Photo input received. Controller=%s Local=%s"),
		*GetNameSafe(this), IsLocalController() ? TEXT("true") : TEXT("false"));
	if (!PhotoCaptureComponent)
	{
		UE_LOG(LogNPPhoto, Error, TEXT("[Input] PhotoCaptureComponent is null."));
		return;
	}
	const bool bStarted = PhotoCaptureComponent->TakePhoto();
	UE_LOG(LogNPPhoto, Log, TEXT("[Input] TakePhoto result=%s"), bStarted ? TEXT("success") : TEXT("failed"));
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
