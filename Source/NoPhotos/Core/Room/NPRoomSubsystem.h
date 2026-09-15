#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NPRoomSubsystem.generated.h"

class APlayerController;
class APlayerState;
class UCanvas;
class UNetDriver;
class UWorld;

//UI
USTRUCT(BlueprintType)
struct NOPHOTOS_API FNPRoomListEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Room")
	int32 RoomNumber = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Room")
	FString HostName;

	UPROPERTY(BlueprintReadOnly, Category = "Room")
	int32 CurrentPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Room")
	int32 MaxPlayers = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFindRoomsCompleteDelegate, const TArray<int32>&, RoomIndices);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNPOnRoomListUpdated, const TArray<FNPRoomListEntry>&, Rooms);

struct FNPRoomDebugMessage
{
	FString Message;
	FLinearColor Color = FLinearColor::White;
	double ExpirationTime = 0.0;
};

enum class ENPRoomExitAction : uint8
{
	None,
	BecomeHost,
	RejoinMigratedRoom
};

DECLARE_DELEGATE_OneParam(FNPOnWaitingRoomRestored, bool);

UCLASS()
class NOPHOTOS_API UNPRoomSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Room")
	bool HostRoom();

	UFUNCTION(BlueprintCallable, Category = "Room")
	bool FindRooms();

	UFUNCTION(BlueprintPure, Category = "Room")
	bool IsFindingRooms() const { return FindSessionsCompleteHandle.IsValid(); }

	UFUNCTION(BlueprintCallable, Category = "Room")
	bool JoinRoom(int32 RoomNumber);

	void SetRoomLevelPath(const FString& LevelPath);
	void RecoverSessionState();
	void LeaveRoom(const FString& MenuLevelPath);
	void BeginHostMigration(const FString& MigrationId, bool bBecomeHost, const FString& MenuLevelPath);

	void UpdateRoomPlayerCount(int32 PlayerCount);
	void MarkRoomInGame();
	/** ServerTravel 직전 현재 대기방 참가자를 메인 월드 준비 대상 목록으로 고정합니다. */
	void CaptureExpectedMainGamePlayers(UWorld* RoomWorld);
	const TSet<FString>& GetExpectedMainGamePlayerIds() const
	{
		return ExpectedMainGamePlayerIds;
	}
	void ClearExpectedMainGamePlayers();
	static FString BuildMainGamePlayerId(const APlayerState* PlayerState);
	void RestoreWaitingRoom(FNPOnWaitingRoomRestored CompletionDelegate);
	bool IsWaitingRoomActive() const;

	void LogOnlineServiceStatus();
	void DisplayDebugMessage(const FString& Message, const FLinearColor& Color);
	bool ConsumeConnectionFailureMessage(FText& OutMessage);

private:
	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleFindSessionsComplete(bool bWasSuccessful);
	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleNetworkFailureSessionCleanupComplete(FName SessionName, bool bWasSuccessful);
	void HandleEndSessionForRoomReturnComplete(FName SessionName, bool bWasSuccessful);
	void HandleUpdateWaitingRoomComplete(FName SessionName, bool bWasSuccessful);
	void HandlePostLoadMap(UWorld* LoadedWorld);
	void UpdateSessionToWaiting();
	void CompleteWaitingRoomRestore(bool bWasSuccessful);
	void BeginSessionCleanup(UWorld* World);
	void BeginExit(ENPRoomExitAction ExitAction, const FString& MigrationId, const FString& MenuLevelPath);
	void TravelToStandaloneMenu();
	void RetryMigrationSearch();
	void ScheduleMigrationSearchRetry();
	void DrawDebugMessages(UCanvas* Canvas, APlayerController* PlayerController);
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);

	TSharedPtr<FOnlineSessionSearch> SessionSearch;
	TArray<int32> ListedRoomResultIndices;
	FDelegateHandle CreateSessionCompleteHandle;
	FDelegateHandle FindSessionsCompleteHandle;
	FDelegateHandle JoinSessionCompleteHandle;
	FDelegateHandle EndSessionForRoomReturnCompleteHandle;
	FDelegateHandle UpdateWaitingRoomCompleteHandle;
	FDelegateHandle DebugDrawHandle;
	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle PostLoadMapHandle;
	FTimerHandle MigrationSearchTimer;
	ENPRoomExitAction PendingExitAction = ENPRoomExitAction::None;
	FString PendingMigrationId;
	FString RoomLevelPath;
	FString ReturnMapPath;
	FNPOnWaitingRoomRestored WaitingRoomRestoredDelegate;
	int32 MigrationSearchAttempts = 0;
	bool bSearchingForMigration = false;
	bool bCleaningSessionAfterNetworkFailure = false;
	bool bHasLoggedOnlineServiceStatus = false;
	FText PendingConnectionFailureMessage;
	TArray<FNPRoomDebugMessage> DebugMessages;
	TSet<FString> ExpectedMainGamePlayerIds;
	
#pragma region UI
public:
	UPROPERTY(BlueprintAssignable, Category = "Room")
	FOnFindRoomsCompleteDelegate OnFindRoomsComplete;

	UPROPERTY(BlueprintAssignable, Category = "Room")
	FNPOnRoomListUpdated OnRoomListUpdated;

	UFUNCTION(BlueprintPure, Category = "Room")
	TArray<FNPRoomListEntry> GetListedRooms() const { return ListedRooms; }
#pragma endregion

private:
	TArray<FNPRoomListEntry> ListedRooms;
};
