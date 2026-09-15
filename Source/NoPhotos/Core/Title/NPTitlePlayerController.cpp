// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/Title/NPTitlePlayerController.h"

#include "Core/Component/NPRoomPlayerComponent.h"
#include "Core/Component/NPPSOPrecacheComponent.h"
#include "Core/Room/NPRoomCheatManager.h"
#include "Engine/GameInstance.h"
#include "SubSystem/NPUIManagerSubsystem.h"
#include "UI/NPUserWidget.h"
#include "UI/Loading/NPMainWorldLoadingWidget.h"

ANPTitlePlayerController::ANPTitlePlayerController()
{
	RoomComponent = CreateDefaultSubobject<UNPRoomPlayerComponent>(TEXT("RoomComponent"));
	PSOPrecacheComponent = CreateDefaultSubobject<UNPPSOPrecacheComponent>(TEXT("PSOPrecacheComponent"));
	CheatClass = UNPRoomCheatManager::StaticClass();
}

bool ANPTitlePlayerController::HostRoom()
{
	return bStartupPSOReady && RoomComponent && RoomComponent->HostRoom();
}

bool ANPTitlePlayerController::FindRooms()
{
	return bStartupPSOReady && RoomComponent && RoomComponent->FindRooms();
}

bool ANPTitlePlayerController::JoinRoom(const int32 RoomNumber)
{
	return bStartupPSOReady && RoomComponent && RoomComponent->JoinRoom(RoomNumber);
}

void ANPTitlePlayerController::ShowMainMenuUI()
{
	if (!IsLocalController())
	{
		return;
	}

	if (!bStartupPSOReady)
	{
		if (bWaitingForStartupPSO)
		{
			return;
		}

		bWaitingForStartupPSO = true;
		StartupLoadingWidget = CreateWidget<UNPMainWorldLoadingWidget>(
			this, UNPMainWorldLoadingWidget::StaticClass());
		if (StartupLoadingWidget)
		{
			StartupLoadingWidget->AddToViewport(10000);
			StartupLoadingWidget->SetLoadingText(
				NSLOCTEXT("NoPhotos", "ShaderCacheLoading", "Shader Cache Loading..."));
		}
		SetIgnoreMoveInput(true);
		SetIgnoreLookInput(true);
		SetInputMode(FInputModeUIOnly());
		PSOPrecacheComponent->WaitForCompletion(
			FSimpleDelegate::CreateUObject(this, &ThisClass::FinishStartupPreparation),
			FNPPSOPrecacheProgress::CreateUObject(
				StartupLoadingWidget.Get(), &UNPMainWorldLoadingWidget::SetShaderCacheProgress));
		return;
	}

	if (!IsValid(MainMenuWidgetClass))
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
	UIManager->PushWidget(MainMenuWidgetClass);
}

void ANPTitlePlayerController::ClientShowMainMenuUI_Implementation()
{
	ShowMainMenuUI();
}

void ANPTitlePlayerController::FinishStartupPreparation()
{
	bWaitingForStartupPSO = false;
	bStartupPSOReady = true;
	if (StartupLoadingWidget)
	{
		StartupLoadingWidget->RemoveFromParent();
		StartupLoadingWidget = nullptr;
	}
	SetIgnoreMoveInput(false);
	SetIgnoreLookInput(false);
	ShowMainMenuUI();
}

void ANPTitlePlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	PSOPrecacheComponent->CancelWait();
	if (StartupLoadingWidget)
	{
		StartupLoadingWidget->RemoveFromParent();
		StartupLoadingWidget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

