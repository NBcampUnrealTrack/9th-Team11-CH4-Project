#include "NPRoomSubsystem.h"

#include "NPRoomLog.h"
#include "Debug/DebugDrawService.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "TimerManager.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace NPRoomSession
{
	const FName ProjectKey(TEXT("NP_PROJECT"));
	const FName RoomStateKey(TEXT("NP_ROOM_STATE"));
	const FName PlayerCountKey(TEXT("NP_PLAYER_COUNT"));
	const FName HostNameKey(TEXT("NP_HOST_NAME"));
	const FName MigrationKey(TEXT("NP_MIGRATION_ID"));
	const FString ProjectValue(TEXT("NoPhotos"));
	const FString WaitingState(TEXT("Waiting"));
	const FString InGameState(TEXT("InGame"));
	constexpr int32 MaxPublicConnections = 6;
	constexpr int32 MaxSearchResults = 100;
	constexpr int32 MaxMigrationSearchAttempts = 15;
}

void UNPRoomSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	DebugDrawHandle = UDebugDrawService::Register(
		TEXT("Game"),
		FDebugDrawDelegate::CreateUObject(this, &UNPRoomSubsystem::DrawDebugMessages));

	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UNPRoomSubsystem::HandleNetworkFailure);
	}

	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
		this,
		&UNPRoomSubsystem::HandlePostLoadMap);
}

void UNPRoomSubsystem::Deinitialize()
{
	if (IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(GetWorld()))
	{
		if (const IOnlineSessionPtr SessionInterface = OnlineSubsystem->GetSessionInterface(); SessionInterface.IsValid())
		{
			if (CreateSessionCompleteHandle.IsValid())
			{
				SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
			}

			if (FindSessionsCompleteHandle.IsValid())
			{
				SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
			}

			if (JoinSessionCompleteHandle.IsValid())
			{
				SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
			}

			if (EndSessionForRoomReturnCompleteHandle.IsValid())
			{
				SessionInterface->ClearOnEndSessionCompleteDelegate_Handle(EndSessionForRoomReturnCompleteHandle);
			}

			if (UpdateWaitingRoomCompleteHandle.IsValid())
			{
				SessionInterface->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateWaitingRoomCompleteHandle);
			}
		}
	}

	if (DebugDrawHandle.IsValid())
	{
		UDebugDrawService::Unregister(DebugDrawHandle);
		DebugDrawHandle.Reset();
	}

	if (GEngine && NetworkFailureHandle.IsValid())
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		NetworkFailureHandle.Reset();
	}

	if (PostLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
		PostLoadMapHandle.Reset();
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MigrationSearchTimer);
	}

	DebugMessages.Reset();
	SessionSearch.Reset();
	ListedRoomResultIndices.Reset();
	ListedRooms.Reset();
	PendingExitAction = ENPRoomExitAction::None;
	PendingMigrationId.Reset();
	RoomLevelPath.Reset();
	ReturnMapPath.Reset();
	WaitingRoomRestoredDelegate.Unbind();
	bCleaningSessionAfterNetworkFailure = false;
	Super::Deinitialize();
}

bool UNPRoomSubsystem::HostRoom()
{
	if (RoomLevelPath.IsEmpty())
	{
		NPRoomLog::Warning(this, TEXT("방 생성 실패: 이동할 RoomLevel이 지정되지 않았습니다."));
		return false;
	}

	if (bCleaningSessionAfterNetworkFailure)
	{
		NPRoomLog::Warning(this, TEXT("방 생성 생략: 연결이 끊긴 이전 방 정보를 정리 중입니다."));
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		NPRoomLog::Warning(this, TEXT("방 생성 실패: World를 찾지 못했습니다."));
		return false;
	}

	if (World->GetNetMode() == NM_ListenServer)
	{
		NPRoomLog::Warning(this, TEXT("방 생성 생략: 현재 월드는 이미 ListenServer입니다."));
		return false;
	}

	if (CreateSessionCompleteHandle.IsValid())
	{
		NPRoomLog::Warning(this, TEXT("방 생성 생략: 이전 온라인 방 생성 요청을 처리 중입니다."));
		return false;
	}

	IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(World);
	const IOnlineSessionPtr SessionInterface = OnlineSubsystem ? OnlineSubsystem->GetSessionInterface() : nullptr;
	if (!SessionInterface.IsValid())
	{
		NPRoomLog::Warning(this, TEXT("방 생성 실패: Online Session Interface를 찾지 못했습니다."));
		return false;
	}

	if (SessionInterface->GetNamedSession(NAME_GameSession))
	{
		NPRoomLog::Warning(this, TEXT("방 생성 실패: 이미 참가 중인 세션이 있습니다."));
		return false;
	}

	FOnlineSessionSettings SessionSettings;
	FString HostName;
	if (const APlayerController* HostPlayer = World->GetFirstPlayerController();
		HostPlayer && HostPlayer->PlayerState)
	{
		HostName = HostPlayer->PlayerState->GetPlayerName();
	}

	SessionSettings.bIsLANMatch = false;
	SessionSettings.NumPublicConnections = NPRoomSession::MaxPublicConnections;
	SessionSettings.bShouldAdvertise = true;
	SessionSettings.bAllowJoinInProgress = true;
	SessionSettings.bAllowInvites = true;
	SessionSettings.bUsesPresence = true;
	SessionSettings.bAllowJoinViaPresence = true;
	SessionSettings.bUseLobbiesIfAvailable = true;
	SessionSettings.Set(
		NPRoomSession::ProjectKey,
		NPRoomSession::ProjectValue,
		EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	SessionSettings.Set(
		NPRoomSession::RoomStateKey,
		NPRoomSession::WaitingState,
		EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	SessionSettings.Set(
		NPRoomSession::PlayerCountKey,
		1,
		EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	SessionSettings.Set(
		NPRoomSession::HostNameKey,
		HostName,
		EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	if (PendingExitAction == ENPRoomExitAction::BecomeHost && !PendingMigrationId.IsEmpty())
	{
		SessionSettings.Set(
			NPRoomSession::MigrationKey,
			PendingMigrationId,
			EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	}

	CreateSessionCompleteHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &UNPRoomSubsystem::HandleCreateSessionComplete));

	NPRoomLog::Info(this, TEXT("온라인 방 생성 요청 중..."));
	if (!SessionInterface->CreateSession(0, NAME_GameSession, SessionSettings))
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
		CreateSessionCompleteHandle.Reset();
		NPRoomLog::Warning(this, TEXT("방 생성 실패: 온라인 세션 요청을 시작하지 못했습니다."));
		return false;
	}

	return true;
}

void UNPRoomSubsystem::SetRoomLevelPath(const FString& LevelPath)
{
	if (!LevelPath.IsEmpty())
	{
		RoomLevelPath = LevelPath;
	}
}

bool UNPRoomSubsystem::FindRooms()
{
	if (bCleaningSessionAfterNetworkFailure)
	{
		NPRoomLog::Warning(this, TEXT("방 목록 검색 생략: 연결이 끊긴 이전 방 정보를 정리 중입니다."));
		return false;
	}

	if (const UWorld* World = GetWorld(); World && World->GetNetMode() == NM_Client)
	{
		NPRoomLog::Warning(this, TEXT("방 목록 검색 생략: 이미 다른 방에 참가 중입니다."));
		return false;
	}

	if (FindSessionsCompleteHandle.IsValid())
	{
		NPRoomLog::Warning(this, TEXT("방 목록 검색 생략: 이전 검색 요청을 처리 중입니다."));
		return false;
	}

	IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(GetWorld());
	const IOnlineSessionPtr SessionInterface = OnlineSubsystem ? OnlineSubsystem->GetSessionInterface() : nullptr;
	if (!SessionInterface.IsValid())
	{
		NPRoomLog::Warning(this, TEXT("방 목록 검색 실패: Online Session Interface를 찾지 못했습니다."));
		return false;
	}

	SessionSearch = MakeShared<FOnlineSessionSearch>();
	ListedRoomResultIndices.Reset();
	ListedRooms.Reset();
	SessionSearch->MaxSearchResults = NPRoomSession::MaxSearchResults;
	SessionSearch->bIsLanQuery = false;
	SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	SessionSearch->QuerySettings.Set(NPRoomSession::ProjectKey, NPRoomSession::ProjectValue, EOnlineComparisonOp::Equals);
	SessionSearch->QuerySettings.Set(NPRoomSession::RoomStateKey, NPRoomSession::WaitingState, EOnlineComparisonOp::Equals);
	if (bSearchingForMigration && !PendingMigrationId.IsEmpty())
	{
		SessionSearch->QuerySettings.Set(
			NPRoomSession::MigrationKey,
			PendingMigrationId,
			EOnlineComparisonOp::Equals);
	}

	FindSessionsCompleteHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &UNPRoomSubsystem::HandleFindSessionsComplete));

	NPRoomLog::Info(
		this,
		bSearchingForMigration
			? TEXT("이전된 호스트의 온라인 방 검색 중...")
			: TEXT("온라인 방 목록 검색 중..."));
	if (!SessionInterface->FindSessions(0, SessionSearch.ToSharedRef()))
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
		FindSessionsCompleteHandle.Reset();
		NPRoomLog::Warning(this, TEXT("방 목록 검색 실패: 온라인 검색 요청을 시작하지 못했습니다."));
		return false;
	}

	return true;
}

bool UNPRoomSubsystem::JoinRoom(const int32 RoomNumber)
{
	if (bCleaningSessionAfterNetworkFailure)
	{
		NPRoomLog::Warning(this, TEXT("방 참가 생략: 연결이 끊긴 이전 방 정보를 정리 중입니다."));
		return false;
	}

	if (const UWorld* World = GetWorld(); World && World->GetNetMode() == NM_Client)
	{
		NPRoomLog::Warning(this, TEXT("방 참가 생략: 이미 다른 방에 참가 중입니다."));
		return false;
	}

	if (JoinSessionCompleteHandle.IsValid())
	{
		NPRoomLog::Warning(this, TEXT("방 참가 생략: 이전 온라인 참가 요청을 처리 중입니다."));
		return false;
	}

	if (RoomNumber <= 0)
	{
		NPRoomLog::Warning(this, TEXT("방 참가 실패: join {방번호} 형식으로 입력해 주세요."));
		return false;
	}

	if (!SessionSearch.IsValid())
	{
		NPRoomLog::Warning(this, TEXT("방 참가 실패: 먼저 list 명령어로 방 목록을 검색해 주세요."));
		return false;
	}

	const int32 ListedRoomIndex = RoomNumber - 1;
	if (!ListedRoomResultIndices.IsValidIndex(ListedRoomIndex))
	{
		NPRoomLog::Warning(
			this,
			FString::Printf(TEXT("방 참가 실패: 최근 list 결과에 방번호 %d이(가) 없습니다."), RoomNumber));
		return false;
	}

	const int32 SearchResultIndex = ListedRoomResultIndices[ListedRoomIndex];
	if (!SessionSearch->SearchResults.IsValidIndex(SearchResultIndex))
	{
		NPRoomLog::Warning(this, TEXT("방 참가 실패: 저장된 검색 결과가 유효하지 않습니다. list를 다시 입력해 주세요."));
		return false;
	}

	const FOnlineSessionSearchResult& SelectedRoom = SessionSearch->SearchResults[SearchResultIndex];

	FString RoomState;
	SelectedRoom.Session.SessionSettings.Get(NPRoomSession::RoomStateKey, RoomState);
	if (RoomState != NPRoomSession::WaitingState)
	{
		NPRoomLog::Warning(this, TEXT("방 참가 실패: 이미 게임이 진행 중인 방입니다."));
		return false;
	}

	int32 CurrentPlayers = 0;
	SelectedRoom.Session.SessionSettings.Get(NPRoomSession::PlayerCountKey, CurrentPlayers);
	const int32 MaxPlayers = SelectedRoom.Session.SessionSettings.NumPublicConnections;
	if (CurrentPlayers >= MaxPlayers)
	{
		NPRoomLog::Warning(this, TEXT("방 참가 실패: 정원이 가득 찬 방입니다."));
		return false;
	}

	IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(GetWorld());
	const IOnlineSessionPtr SessionInterface = OnlineSubsystem ? OnlineSubsystem->GetSessionInterface() : nullptr;
	if (!SessionInterface.IsValid())
	{
		NPRoomLog::Warning(this, TEXT("방 참가 실패: Online Session Interface를 찾지 못했습니다."));
		return false;
	}

	JoinSessionCompleteHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &UNPRoomSubsystem::HandleJoinSessionComplete));

	NPRoomLog::Info(
		this,
		FString::Printf(
			TEXT("온라인 방 참가 요청 중: Room=%d, ID=%s"),
			RoomNumber,
			*SelectedRoom.GetSessionIdStr()));
	if (!SessionInterface->JoinSession(0, NAME_GameSession, SelectedRoom))
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
		JoinSessionCompleteHandle.Reset();
		NPRoomLog::Warning(this, TEXT("방 참가 실패: 온라인 참가 요청을 시작하지 못했습니다."));
		return false;
	}

	return true;
}

void UNPRoomSubsystem::LeaveRoom(const FString& MenuLevelPath)
{
	BeginExit(ENPRoomExitAction::None, FString(), MenuLevelPath);
}

void UNPRoomSubsystem::BeginHostMigration(
	const FString& MigrationId,
	const bool bBecomeHost,
	const FString& MenuLevelPath)
{
	if (MigrationId.IsEmpty())
	{
		NPRoomLog::Warning(this, TEXT("호스트 이전 실패: 이전 토큰이 비어 있습니다."));
		LeaveRoom(MenuLevelPath);
		return;
	}

	BeginExit(
		bBecomeHost ? ENPRoomExitAction::BecomeHost : ENPRoomExitAction::RejoinMigratedRoom,
		MigrationId,
		MenuLevelPath);
}

void UNPRoomSubsystem::BeginExit(
	const ENPRoomExitAction ExitAction,
	const FString& MigrationId,
	const FString& MenuLevelPath)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		NPRoomLog::Warning(this, TEXT("방 나가기 실패: World를 찾지 못했습니다."));
		return;
	}

	PendingExitAction = ExitAction;
	PendingMigrationId = MigrationId;
	ReturnMapPath = MenuLevelPath;
	MigrationSearchAttempts = 0;
	bSearchingForMigration = false;
	World->GetTimerManager().ClearTimer(MigrationSearchTimer);

	if (ExitAction == ENPRoomExitAction::BecomeHost)
	{
		NPRoomLog::Info(this, TEXT("다음 호스트로 선정되었습니다. 새 방을 준비합니다."));
	}
	else if (ExitAction == ENPRoomExitAction::RejoinMigratedRoom)
	{
		NPRoomLog::Info(this, TEXT("호스트가 변경됩니다. 새 호스트의 방으로 재접속합니다."));
	}

	IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(World);
	const IOnlineSessionPtr SessionInterface = OnlineSubsystem ? OnlineSubsystem->GetSessionInterface() : nullptr;
	if (!SessionInterface.IsValid() || !SessionInterface->GetNamedSession(NAME_GameSession))
	{
		TravelToStandaloneMenu();
		return;
	}

	const FOnDestroySessionCompleteDelegate DestroyCompleteDelegate =
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &UNPRoomSubsystem::HandleDestroySessionComplete);
	if (!SessionInterface->DestroySession(NAME_GameSession, DestroyCompleteDelegate))
	{
		NPRoomLog::Warning(this, TEXT("온라인 세션 나가기 요청 실패: 메뉴로 이동을 계속합니다."));
		TravelToStandaloneMenu();
	}
}

void UNPRoomSubsystem::HandleDestroySessionComplete(const FName SessionName, const bool bWasSuccessful)
{
	if (SessionName != NAME_GameSession)
	{
		return;
	}

	NPRoomLog::Info(
		this,
		bWasSuccessful
			? TEXT("온라인 방 나가기 완료")
			: TEXT("온라인 방 나가기 결과를 확인하지 못했지만 메뉴 이동을 계속합니다."));
	TravelToStandaloneMenu();
}

void UNPRoomSubsystem::HandleNetworkFailureSessionCleanupComplete(
	const FName SessionName,
	const bool bWasSuccessful)
{
	if (SessionName != NAME_GameSession)
	{
		return;
	}

	bCleaningSessionAfterNetworkFailure = false;

	if (!bWasSuccessful)
	{
		if (IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(GetWorld()))
		{
			if (const IOnlineSessionPtr SessionInterface = OnlineSubsystem->GetSessionInterface(); SessionInterface.IsValid())
			{
				SessionInterface->RemoveNamedSession(NAME_GameSession);
			}
		}

		NPRoomLog::Warning(this, TEXT("이전 온라인 방의 종료 응답을 확인하지 못해 로컬 방 정보를 제거했습니다."));
	}

	NPRoomLog::Info(this, TEXT("연결이 끊긴 이전 방 정보 정리 완료. list로 새 방을 검색할 수 있습니다."));
}

void UNPRoomSubsystem::TravelToStandaloneMenu()
{
	if (ReturnMapPath.IsEmpty())
	{
		NPRoomLog::Warning(this, TEXT("방 나가기 실패: 돌아갈 메뉴 맵을 찾지 못했습니다."));
		return;
	}

	UGameplayStatics::OpenLevel(this, FName(*ReturnMapPath), true);
}

void UNPRoomSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (!LoadedWorld || LoadedWorld->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	if (LoadedWorld->GetNetMode() != NM_Standalone)
	{
		return;
	}

	if (PendingExitAction == ENPRoomExitAction::BecomeHost)
	{
		NPRoomLog::Info(this, TEXT("이전받은 호스트 방 생성 중..."));
		if (!HostRoom())
		{
			NPRoomLog::Warning(this, TEXT("호스트 이전 실패: 새 온라인 방 생성 요청을 시작하지 못했습니다."));
			PendingExitAction = ENPRoomExitAction::None;
			PendingMigrationId.Reset();
		}
		return;
	}

	if (PendingExitAction == ENPRoomExitAction::RejoinMigratedRoom)
	{
		RetryMigrationSearch();
	}
}

void UNPRoomSubsystem::RetryMigrationSearch()
{
	if (PendingExitAction != ENPRoomExitAction::RejoinMigratedRoom)
	{
		return;
	}

	++MigrationSearchAttempts;
	bSearchingForMigration = true;
	if (!FindRooms())
	{
		bSearchingForMigration = false;
		ScheduleMigrationSearchRetry();
	}
}

void UNPRoomSubsystem::ScheduleMigrationSearchRetry()
{
	if (MigrationSearchAttempts >= NPRoomSession::MaxMigrationSearchAttempts)
	{
		NPRoomLog::Warning(this, TEXT("호스트 이전 실패: 새 호스트의 방을 찾지 못했습니다."));
		PendingExitAction = ENPRoomExitAction::None;
		PendingMigrationId.Reset();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			MigrationSearchTimer,
			this,
			&UNPRoomSubsystem::RetryMigrationSearch,
			1.0f,
			false);
	}
}

void UNPRoomSubsystem::UpdateRoomPlayerCount(const int32 PlayerCount)
{
	IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(GetWorld());
	const IOnlineSessionPtr SessionInterface = OnlineSubsystem ? OnlineSubsystem->GetSessionInterface() : nullptr;
	FOnlineSessionSettings* SessionSettings = SessionInterface.IsValid()
		? SessionInterface->GetSessionSettings(NAME_GameSession)
		: nullptr;
	if (!SessionSettings)
	{
		NPRoomLog::Warning(this, TEXT("방 인원 갱신 실패: 생성된 온라인 세션을 찾지 못했습니다."));
		return;
	}

	const int32 ClampedPlayerCount = FMath::Clamp(PlayerCount, 1, NPRoomSession::MaxPublicConnections);
	SessionSettings->Set(
		NPRoomSession::PlayerCountKey,
		ClampedPlayerCount,
		EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	if (!SessionInterface->UpdateSession(NAME_GameSession, *SessionSettings, true))
	{
		NPRoomLog::Warning(this, TEXT("방 인원 갱신 실패: UpdateSession 요청을 시작하지 못했습니다."));
		return;
	}

	NPRoomLog::Info(
		this,
		FString::Printf(
			TEXT("온라인 방 인원 갱신 요청: Player=%d/%d"),
			ClampedPlayerCount,
			NPRoomSession::MaxPublicConnections));
}

void UNPRoomSubsystem::MarkRoomInGame()
{
	IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(GetWorld());
	const IOnlineSessionPtr SessionInterface = OnlineSubsystem ? OnlineSubsystem->GetSessionInterface() : nullptr;
	FOnlineSessionSettings* SessionSettings = SessionInterface.IsValid()
		? SessionInterface->GetSessionSettings(NAME_GameSession)
		: nullptr;
	if (!SessionSettings)
	{
		NPRoomLog::Warning(this, TEXT("세션 상태 변경 실패: 생성된 온라인 세션을 찾지 못했습니다."));
		return;
	}

	SessionSettings->Set(
		NPRoomSession::RoomStateKey,
		NPRoomSession::InGameState,
		EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	SessionSettings->bAllowJoinInProgress = false;
	SessionSettings->bAllowInvites = false;
	SessionSettings->bAllowJoinViaPresence = false;
	SessionSettings->bShouldAdvertise = false;

	const bool bUpdateRequested = SessionInterface->UpdateSession(NAME_GameSession, *SessionSettings, true);
	if (!bUpdateRequested)
	{
		NPRoomLog::Warning(this, TEXT("세션 상태 변경 실패: UpdateSession 요청을 시작하지 못했습니다."));
	}

	const bool bStartRequested = SessionInterface->StartSession(NAME_GameSession);
	if (!bStartRequested)
	{
		NPRoomLog::Warning(this, TEXT("세션 시작 상태 변경 실패: StartSession 요청을 시작하지 못했습니다."));
	}

	if (bUpdateRequested && bStartRequested)
	{
		NPRoomLog::Info(this, TEXT("온라인 방 상태 변경 요청: InGame, 신규 참가 및 목록 노출 차단"));
	}
}

void UNPRoomSubsystem::HandleCreateSessionComplete(const FName SessionName, const bool bWasSuccessful)
{
	IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(GetWorld());
	const IOnlineSessionPtr SessionInterface = OnlineSubsystem ? OnlineSubsystem->GetSessionInterface() : nullptr;
	if (SessionInterface.IsValid() && CreateSessionCompleteHandle.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
		CreateSessionCompleteHandle.Reset();
	}

	if (!bWasSuccessful || SessionName != NAME_GameSession)
	{
		NPRoomLog::Warning(this, TEXT("방 생성 실패: 온라인 세션 생성이 완료되지 않았습니다."));
		if (PendingExitAction == ENPRoomExitAction::BecomeHost)
		{
			PendingExitAction = ENPRoomExitAction::None;
			PendingMigrationId.Reset();
		}
		return;
	}

	const FNamedOnlineSession* NamedSession = SessionInterface.IsValid()
		? SessionInterface->GetNamedSession(NAME_GameSession)
		: nullptr;
	const FString RoomId = NamedSession ? NamedSession->GetSessionIdStr() : TEXT("Unknown");
	NPRoomLog::Info(
		this,
		FString::Printf(
			TEXT("온라인 방 생성 완료: ID=%s, Player=1/%d"),
			*RoomId,
			NPRoomSession::MaxPublicConnections));

	if (PendingExitAction == ENPRoomExitAction::BecomeHost)
	{
		NPRoomLog::Info(this, TEXT("호스트로 지정되었습니다. 게스트 참가 후 start를 입력해 주세요."));
		PendingExitAction = ENPRoomExitAction::None;
		PendingMigrationId.Reset();
	}

	NPRoomLog::Info(this, FString::Printf(TEXT("대기방 이동: OpenLevel -> %s?listen"), *RoomLevelPath));
	UGameplayStatics::OpenLevel(this, FName(*RoomLevelPath), true, TEXT("listen"));
}

void UNPRoomSubsystem::HandleFindSessionsComplete(const bool bWasSuccessful)
{
	const bool bWasMigrationSearch = bSearchingForMigration;
	IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(GetWorld());
	const IOnlineSessionPtr SessionInterface = OnlineSubsystem ? OnlineSubsystem->GetSessionInterface() : nullptr;
	if (SessionInterface.IsValid() && FindSessionsCompleteHandle.IsValid())
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
		FindSessionsCompleteHandle.Reset();
	}

	if (!bWasSuccessful || !SessionSearch.IsValid())
	{
		bSearchingForMigration = false;
		if (bWasMigrationSearch)
		{
			ScheduleMigrationSearchRetry();
		}
		else
		{
			NPRoomLog::Warning(this, TEXT("방 목록 검색 실패: 온라인 검색이 정상적으로 완료되지 않았습니다."));
			OnFindRoomsComplete.Broadcast(ListedRoomResultIndices);
			OnRoomListUpdated.Broadcast(ListedRooms);
		}
		return;
	}

	int32 VisibleRoomCount = 0;
	ListedRoomResultIndices.Reset();
	ListedRooms.Reset();
	for (int32 SearchResultIndex = 0; SearchResultIndex < SessionSearch->SearchResults.Num(); ++SearchResultIndex)
	{
		const FOnlineSessionSearchResult& SearchResult = SessionSearch->SearchResults[SearchResultIndex];
		FString ProjectValue;
		FString RoomState;
		FString HostName;
		int32 CurrentPlayers = 0;
		SearchResult.Session.SessionSettings.Get(NPRoomSession::ProjectKey, ProjectValue);
		SearchResult.Session.SessionSettings.Get(NPRoomSession::RoomStateKey, RoomState);
		SearchResult.Session.SessionSettings.Get(NPRoomSession::PlayerCountKey, CurrentPlayers);
		SearchResult.Session.SessionSettings.Get(NPRoomSession::HostNameKey, HostName);
		const int32 MaxPlayers = SearchResult.Session.SessionSettings.NumPublicConnections;

		if (!SearchResult.IsValid()
			|| ProjectValue != NPRoomSession::ProjectValue
			|| RoomState != NPRoomSession::WaitingState
			|| CurrentPlayers >= MaxPlayers)
		{
			continue;
		}

		ListedRoomResultIndices.Add(SearchResultIndex);
		const int32 RoomNumber = ListedRoomResultIndices.Num();
		FNPRoomListEntry& Room = ListedRooms.AddDefaulted_GetRef();
		Room.RoomNumber = RoomNumber;
		Room.HostName = HostName.IsEmpty() ? SearchResult.Session.OwningUserName : HostName;
		Room.CurrentPlayers = CurrentPlayers;
		Room.MaxPlayers = MaxPlayers;
		if (!bWasMigrationSearch)
		{
			NPRoomLog::Info(
				this,
				FString::Printf(
					TEXT("방 목록: Room=%d, Host=%s, Player=%d/%d"),
					RoomNumber,
					*Room.HostName,
					CurrentPlayers,
					MaxPlayers));
		}
		++VisibleRoomCount;
	}

	if (bWasMigrationSearch)
	{
		bSearchingForMigration = false;
		if (VisibleRoomCount > 0)
		{
			NPRoomLog::Info(this, TEXT("새 호스트의 방을 찾았습니다. 자동으로 참가합니다."));
			JoinRoom(1);
		}
		else
		{
			ScheduleMigrationSearchRetry();
		}
		return;
	}

	if (VisibleRoomCount == 0)
	{
		NPRoomLog::Info(this, TEXT("방 목록: 참가 가능한 대기방이 없습니다."));
	}
	
	OnFindRoomsComplete.Broadcast(ListedRoomResultIndices);
	OnRoomListUpdated.Broadcast(ListedRooms);
}

void UNPRoomSubsystem::HandleJoinSessionComplete(
	const FName SessionName,
	const EOnJoinSessionCompleteResult::Type Result)
{
	IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(GetWorld());
	const IOnlineSessionPtr SessionInterface = OnlineSubsystem ? OnlineSubsystem->GetSessionInterface() : nullptr;
	if (SessionInterface.IsValid() && JoinSessionCompleteHandle.IsValid())
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
		JoinSessionCompleteHandle.Reset();
	}

	if (Result != EOnJoinSessionCompleteResult::Success || !SessionInterface.IsValid())
	{
		const FString FailureReason = Result == EOnJoinSessionCompleteResult::SessionIsFull
			? TEXT("정원이 가득 찼습니다.")
			: Result == EOnJoinSessionCompleteResult::SessionDoesNotExist
				? TEXT("방이 사라졌거나 게임이 이미 시작되었습니다.")
				: FString::Printf(TEXT("온라인 서비스 오류=%s"), LexToString(Result));
		NPRoomLog::Warning(this, FString::Printf(TEXT("방 참가 실패: %s"), *FailureReason));
		return;
	}

	FString ConnectString;
	if (!SessionInterface->GetResolvedConnectString(SessionName, ConnectString))
	{
		NPRoomLog::Warning(this, TEXT("방 참가 실패: 온라인 접속 주소를 확인하지 못했습니다."));
		return;
	}

	APlayerController* PlayerController = GetGameInstance()
		? GetGameInstance()->GetFirstLocalPlayerController()
		: nullptr;
	if (!PlayerController)
	{
		NPRoomLog::Warning(this, TEXT("방 참가 실패: 로컬 PlayerController를 찾지 못했습니다."));
		return;
	}

	NPRoomLog::Info(this, TEXT("온라인 방 참가 승인: 서버로 이동합니다."));
	if (PendingExitAction == ENPRoomExitAction::RejoinMigratedRoom)
	{
		PendingExitAction = ENPRoomExitAction::None;
		PendingMigrationId.Reset();
	}
	PlayerController->ClientTravel(ConnectString, TRAVEL_Absolute);
}

void UNPRoomSubsystem::LogOnlineServiceStatus()
{
	if (bHasLoggedOnlineServiceStatus)
	{
		return;
	}

	IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(GetWorld());
	if (!OnlineSubsystem)
	{
		NPRoomLog::Warning(this, TEXT("온라인 서비스를 초기화하지 못했습니다. 방 생성 및 검색을 사용할 수 없습니다."));
		bHasLoggedOnlineServiceStatus = true;
		return;
	}

	const FName SubsystemName = OnlineSubsystem->GetSubsystemName();
	if (SubsystemName == FName(TEXT("STEAM")))
	{
		NPRoomLog::Info(this, TEXT("온라인 서비스: Steam 연결 정상. Steam 방 생성 및 목록 검색을 사용합니다."));
	}
	else if (SubsystemName == FName(TEXT("NULL")))
	{
		NPRoomLog::Warning(
			this,
			TEXT("온라인 서비스: 로컬(NULL) 모드. Steam 클라이언트 미실행 또는 Steam 초기화 실패로 Steam 전역 방 목록을 사용할 수 없습니다."));
	}
	else
	{
		NPRoomLog::Info(
			this,
			FString::Printf(TEXT("온라인 서비스: %s 연결 정상."), *SubsystemName.ToString()));
	}

	bHasLoggedOnlineServiceStatus = true;
}

void UNPRoomSubsystem::DisplayDebugMessage(const FString& Message, const FLinearColor& Color)
{
	FNPRoomDebugMessage& DebugMessage = DebugMessages.AddDefaulted_GetRef();
	DebugMessage.Message = Message;
	DebugMessage.Color = Color;
	DebugMessage.ExpirationTime = FPlatformTime::Seconds() + 8.0;
}

void UNPRoomSubsystem::DrawDebugMessages(UCanvas* Canvas, APlayerController* PlayerController)
{
	if (!Canvas || !PlayerController || PlayerController->GetGameInstance() != GetGameInstance() || !GEngine)
	{
		return;
	}

	const double CurrentTime = FPlatformTime::Seconds();
	DebugMessages.RemoveAll(
		[CurrentTime](const FNPRoomDebugMessage& DebugMessage)
		{
			return DebugMessage.ExpirationTime <= CurrentTime;
		});

	float ScreenY = 50.0f;
	for (const FNPRoomDebugMessage& DebugMessage : DebugMessages)
	{
		//Canvas->SetDrawColor(DebugMessage.Color.ToFColor(true));
		//Canvas->DrawText(GEngine->GetSmallFont(), DebugMessage.Message, 40.0f, ScreenY);
		ScreenY += 20.0f;
	}
}

void UNPRoomSubsystem::HandleNetworkFailure(
	UWorld* World,
	UNetDriver* NetDriver,
	const ENetworkFailure::Type FailureType,
	const FString& ErrorString)
{
	UWorld* FailureWorld = World ? World : NetDriver ? NetDriver->GetWorld() : nullptr;
	if (!FailureWorld || !FailureWorld->GetGameInstance())
	{
		FailureWorld = GetWorld();
	}
	if (!FailureWorld || FailureWorld->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	FString FailureMessage;
	switch (FailureType)
	{
	case ENetworkFailure::ConnectionTimeout:
		FailureMessage = TEXT("접속 실패: 서버 응답 시간이 초과되었습니다.");
		PendingConnectionFailureMessage = FText::FromString(
			TEXT("방에 연결하지 못했습니다. 호스트의 응답 시간이 초과되었습니다."));
		break;
	case ENetworkFailure::PendingConnectionFailure:
		FailureMessage = TEXT("접속 실패: 서버에 연결할 수 없습니다.");
		PendingConnectionFailureMessage = FText::FromString(
			TEXT("방에 연결하지 못했습니다. 호스트와 연결할 수 없습니다."));
		break;
	case ENetworkFailure::ConnectionLost:
		FailureMessage = TEXT("서버와의 연결이 끊어졌습니다.");
		PendingConnectionFailureMessage = FText::FromString(
			TEXT("호스트와의 연결이 끊어졌습니다."));
		break;
	case ENetworkFailure::NetDriverCreateFailure:
		FailureMessage = TEXT("네트워크 초기화 실패: NetDriver를 생성하지 못했습니다.");
		break;
	case ENetworkFailure::NetDriverListenFailure:
		FailureMessage = TEXT("방 생성 실패: 서버 포트를 열지 못했습니다.");
		break;
	default:
		FailureMessage = FString::Printf(
			TEXT("네트워크 오류: %s"),
			ENetworkFailure::ToString(FailureType));
		break;
	}

	if (!ErrorString.IsEmpty())
	{
		FailureMessage += FString::Printf(TEXT(" (%s)"), *ErrorString);
	}

	NPRoomLog::Warning(this, FailureMessage);

	const bool bShouldCleanupSession = FailureType == ENetworkFailure::ConnectionTimeout
		|| FailureType == ENetworkFailure::PendingConnectionFailure
		|| FailureType == ENetworkFailure::ConnectionLost;
	if (!bShouldCleanupSession || bCleaningSessionAfterNetworkFailure)
	{
		return;
	}

	IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(FailureWorld);
	const IOnlineSessionPtr SessionInterface = OnlineSubsystem ? OnlineSubsystem->GetSessionInterface() : nullptr;
	if (!SessionInterface.IsValid() || !SessionInterface->GetNamedSession(NAME_GameSession))
	{
		return;
	}

	SessionSearch.Reset();
	ListedRoomResultIndices.Reset();
	ListedRooms.Reset();
	bSearchingForMigration = false;
	FailureWorld->GetTimerManager().ClearTimer(MigrationSearchTimer);
	PendingExitAction = ENPRoomExitAction::None;
	PendingMigrationId.Reset();

	bCleaningSessionAfterNetworkFailure = true;
	NPRoomLog::Info(this, TEXT("연결이 끊긴 이전 온라인 방 정보 정리 중..."));

	const FOnDestroySessionCompleteDelegate CleanupCompleteDelegate =
		FOnDestroySessionCompleteDelegate::CreateUObject(
			this,
			&UNPRoomSubsystem::HandleNetworkFailureSessionCleanupComplete);
	if (!SessionInterface->DestroySession(NAME_GameSession, CleanupCompleteDelegate))
	{
		SessionInterface->RemoveNamedSession(NAME_GameSession);
		bCleaningSessionAfterNetworkFailure = false;
		NPRoomLog::Warning(this, TEXT("이전 온라인 방 정리 요청을 시작하지 못해 로컬 방 정보를 제거했습니다."));
		NPRoomLog::Info(this, TEXT("이제 list로 새 방을 검색할 수 있습니다."));
	}
}

bool UNPRoomSubsystem::ConsumeConnectionFailureMessage(FText& OutMessage)
{
	if (PendingConnectionFailureMessage.IsEmpty())
	{
		return false;
	}

	OutMessage = PendingConnectionFailureMessage;
	PendingConnectionFailureMessage = FText::GetEmpty();
	return true;
}

void UNPRoomSubsystem::RestoreWaitingRoom(FNPOnWaitingRoomRestored CompletionDelegate)
{
	if (WaitingRoomRestoredDelegate.IsBound())
	{
		CompletionDelegate.ExecuteIfBound(false);
		return;
	}

	IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(GetWorld());
	const IOnlineSessionPtr SessionInterface = OnlineSubsystem ? OnlineSubsystem->GetSessionInterface() : nullptr;
	if (!SessionInterface.IsValid() || !SessionInterface->GetNamedSession(NAME_GameSession))
	{
		NPRoomLog::Warning(this, TEXT("대기방 복귀 실패: 유지 중인 온라인 세션을 찾지 못했습니다."));
		CompletionDelegate.ExecuteIfBound(false);
		return;
	}

	WaitingRoomRestoredDelegate = CompletionDelegate;
	if (SessionInterface->GetSessionState(NAME_GameSession) != EOnlineSessionState::InProgress)
	{
		UpdateSessionToWaiting();
		return;
	}

	EndSessionForRoomReturnCompleteHandle = SessionInterface->AddOnEndSessionCompleteDelegate_Handle(
		FOnEndSessionCompleteDelegate::CreateUObject(
			this,
			&UNPRoomSubsystem::HandleEndSessionForRoomReturnComplete));
	if (!SessionInterface->EndSession(NAME_GameSession))
	{
		if (EndSessionForRoomReturnCompleteHandle.IsValid())
		{
			SessionInterface->ClearOnEndSessionCompleteDelegate_Handle(EndSessionForRoomReturnCompleteHandle);
			EndSessionForRoomReturnCompleteHandle.Reset();
		}
		NPRoomLog::Warning(this, TEXT("대기방 복귀 실패: EndSession 요청을 시작하지 못했습니다."));
		CompleteWaitingRoomRestore(false);
	}
}

bool UNPRoomSubsystem::IsWaitingRoomActive() const
{
	IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(GetWorld());
	const IOnlineSessionPtr SessionInterface = OnlineSubsystem ? OnlineSubsystem->GetSessionInterface() : nullptr;
	const FNamedOnlineSession* NamedSession = SessionInterface.IsValid()
		? SessionInterface->GetNamedSession(NAME_GameSession)
		: nullptr;
	if (!NamedSession)
	{
		return false;
	}

	FString RoomState;
	NamedSession->SessionSettings.Get(NPRoomSession::RoomStateKey, RoomState);
	return RoomState == NPRoomSession::WaitingState;
}

void UNPRoomSubsystem::HandleEndSessionForRoomReturnComplete(
	const FName SessionName,
	const bool bWasSuccessful)
{
	IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(GetWorld());
	const IOnlineSessionPtr SessionInterface = OnlineSubsystem ? OnlineSubsystem->GetSessionInterface() : nullptr;
	if (SessionInterface.IsValid() && EndSessionForRoomReturnCompleteHandle.IsValid())
	{
		SessionInterface->ClearOnEndSessionCompleteDelegate_Handle(EndSessionForRoomReturnCompleteHandle);
		EndSessionForRoomReturnCompleteHandle.Reset();
	}

	if (!bWasSuccessful)
	{
		NPRoomLog::Warning(this, TEXT("대기방 복귀 실패: 온라인 세션 종료 상태 전환에 실패했습니다."));
		CompleteWaitingRoomRestore(false);
		return;
	}

	UpdateSessionToWaiting();
}

void UNPRoomSubsystem::UpdateSessionToWaiting()
{
	IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(GetWorld());
	const IOnlineSessionPtr SessionInterface = OnlineSubsystem ? OnlineSubsystem->GetSessionInterface() : nullptr;
	FOnlineSessionSettings* SessionSettings = SessionInterface.IsValid()
		? SessionInterface->GetSessionSettings(NAME_GameSession)
		: nullptr;
	if (!SessionInterface.IsValid() || !SessionSettings)
	{
		NPRoomLog::Warning(this, TEXT("대기방 복귀 실패: 갱신할 온라인 세션 설정을 찾지 못했습니다."));
		CompleteWaitingRoomRestore(false);
		return;
	}

	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	const int32 PlayerCount = GameState ? GameState->PlayerArray.Num() : 0;
	SessionSettings->Set(
		NPRoomSession::RoomStateKey,
		NPRoomSession::WaitingState,
		EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	SessionSettings->Set(
		NPRoomSession::PlayerCountKey,
		FMath::Clamp(PlayerCount, 0, NPRoomSession::MaxPublicConnections),
		EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	SessionSettings->bAllowJoinInProgress = true;
	SessionSettings->bAllowInvites = true;
	SessionSettings->bAllowJoinViaPresence = true;
	SessionSettings->bShouldAdvertise = true;

	UpdateWaitingRoomCompleteHandle = SessionInterface->AddOnUpdateSessionCompleteDelegate_Handle(
		FOnUpdateSessionCompleteDelegate::CreateUObject(
			this,
			&UNPRoomSubsystem::HandleUpdateWaitingRoomComplete));
	if (!SessionInterface->UpdateSession(NAME_GameSession, *SessionSettings, true))
	{
		if (UpdateWaitingRoomCompleteHandle.IsValid())
		{
			SessionInterface->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateWaitingRoomCompleteHandle);
			UpdateWaitingRoomCompleteHandle.Reset();
		}
		NPRoomLog::Warning(this, TEXT("대기방 복귀 실패: Waiting 세션 갱신 요청을 시작하지 못했습니다."));
		CompleteWaitingRoomRestore(false);
	}
}

void UNPRoomSubsystem::HandleUpdateWaitingRoomComplete(
	const FName SessionName,
	const bool bWasSuccessful)
{
	IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(GetWorld());
	const IOnlineSessionPtr SessionInterface = OnlineSubsystem ? OnlineSubsystem->GetSessionInterface() : nullptr;
	if (SessionInterface.IsValid() && UpdateWaitingRoomCompleteHandle.IsValid())
	{
		SessionInterface->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateWaitingRoomCompleteHandle);
		UpdateWaitingRoomCompleteHandle.Reset();
	}

	if (bWasSuccessful)
	{
		NPRoomLog::Info(this, TEXT("온라인 방 상태 복구 완료: Waiting, 신규 참가 및 목록 노출 허용"));
	}
	else
	{
		NPRoomLog::Warning(this, TEXT("대기방 복귀 실패: Waiting 세션 설정 갱신에 실패했습니다."));
	}

	CompleteWaitingRoomRestore(bWasSuccessful);
}

void UNPRoomSubsystem::CompleteWaitingRoomRestore(const bool bWasSuccessful)
{
	FNPOnWaitingRoomRestored CompletionDelegate = WaitingRoomRestoredDelegate;
	WaitingRoomRestoredDelegate.Unbind();
	CompletionDelegate.ExecuteIfBound(bWasSuccessful);
}
