#include "NPRoomCheatManager.h"

#include "NPRoomLog.h"
#include "Core/Component/NPRoomPlayerComponent.h"
#include "Core/Main/NPMainPlayerController.h"
#include "GameFramework/PlayerController.h"

void UNPRoomCheatManager::Create()
{
	if (UNPRoomPlayerComponent* RoomComponent = GetRoomComponent())
	{
		RoomComponent->HostRoom();
		return;
	}

	NPRoomLog::Warning(this, TEXT("create 실패: RoomComponent를 찾지 못했습니다."));
}

void UNPRoomCheatManager::List()
{
	if (UNPRoomPlayerComponent* RoomComponent = GetRoomComponent())
	{
		RoomComponent->FindRooms();
		return;
	}

	NPRoomLog::Warning(this, TEXT("list 실패: RoomComponent를 찾지 못했습니다."));
}

void UNPRoomCheatManager::Join(const int32 RoomNumber)
{
	if (UNPRoomPlayerComponent* RoomComponent = GetRoomComponent())
	{
		RoomComponent->JoinRoom(RoomNumber);
		return;
	}

	NPRoomLog::Warning(this, TEXT("join 실패: RoomComponent를 찾지 못했습니다."));
}

void UNPRoomCheatManager::User()
{
	if (UNPRoomPlayerComponent* RoomComponent = GetRoomComponent())
	{
		RoomComponent->ShowRoomUsers();
		return;
	}

	NPRoomLog::Warning(this, TEXT("user 실패: RoomComponent를 찾지 못했습니다."));
}

void UNPRoomCheatManager::Start()
{
	APlayerController* PlayerController = GetPlayerController();
	UNPRoomPlayerComponent* RoomComponent = GetRoomComponent();
	if (!RoomComponent)
	{
		NPRoomLog::Warning(this, TEXT("start 실패: RoomComponent를 찾지 못했습니다."));
		return;
	}

	if (!RoomComponent->IsRoomHost())
	{
		NPRoomLog::Warning(PlayerController, TEXT("게스트는 start 명령어를 사용할 수 없습니다."));
		return;
	}

	RoomComponent->RequestStartGame();
}

void UNPRoomCheatManager::Rehost()
{
	APlayerController* PlayerController = GetPlayerController();
	UNPRoomPlayerComponent* RoomComponent = GetRoomComponent();
	if (!RoomComponent)
	{
		NPRoomLog::Warning(this, TEXT("rehost 실패: RoomComponent를 찾지 못했습니다."));
		return;
	}

	if (!RoomComponent->IsRoomHost())
	{
		NPRoomLog::Warning(PlayerController, TEXT("게스트는 rehost 명령어를 사용할 수 없습니다."));
		return;
	}

	RoomComponent->RequestRestartRoom();
}

void UNPRoomCheatManager::Out(const FString& Command)
{
	if (!Command.Equals(TEXT("game"), ESearchCase::IgnoreCase))
	{
		NPRoomLog::Warning(this, TEXT("out 실패: out game 형식으로 입력해 주세요."));
		return;
	}

	if (UNPRoomPlayerComponent* RoomComponent = GetRoomComponent())
	{
		RoomComponent->ExitRoom();
		return;
	}

	NPRoomLog::Warning(this, TEXT("out game 실패: RoomComponent를 찾지 못했습니다."));
}

void UNPRoomCheatManager::Add(const FString& Command)
{
	if (!Command.Equals(TEXT("point"), ESearchCase::IgnoreCase))
	{
		NPRoomLog::Warning(this, TEXT("add 실패: add point 형식으로 입력해 주세요."));
		return;
	}

	if (ANPMainPlayerController* PlayerController =
		Cast<ANPMainPlayerController>(GetPlayerController()))
	{
		PlayerController->ServerAddCheatPoint();
		return;
	}

	NPRoomLog::Warning(this, TEXT("add point 실패: 메인 게임 PlayerController가 아닙니다."));
}

void UNPRoomCheatManager::Remove(const FString& Command)
{
	if (!Command.Equals(TEXT("point"), ESearchCase::IgnoreCase))
	{
		NPRoomLog::Warning(this, TEXT("remove 실패: remove point 형식으로 입력해 주세요."));
		return;
	}

	if (ANPMainPlayerController* PlayerController =
		Cast<ANPMainPlayerController>(GetPlayerController()))
	{
		PlayerController->ServerRemoveCheatPoint();
		return;
	}

	NPRoomLog::Warning(this, TEXT("remove point 실패: 메인 게임 PlayerController가 아닙니다."));
}

UNPRoomPlayerComponent* UNPRoomCheatManager::GetRoomComponent() const
{
	APlayerController* PlayerController = GetPlayerController();
	return PlayerController
		? PlayerController->FindComponentByClass<UNPRoomPlayerComponent>()
		: nullptr;
}
