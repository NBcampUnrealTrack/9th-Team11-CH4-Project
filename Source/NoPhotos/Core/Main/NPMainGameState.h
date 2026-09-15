#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "Gameplay/Photo/NPPhotoEvidenceTypes.h"
#include "NPMainGameState.generated.h"

class APlayerController;
class APlayerState;
class ANPPlayerState;

UENUM(BlueprintType)
enum class ENPMainWorldState : uint8
{
	Preparing,
	WaitingForPlayers,
	Countdown,
	Playing,
	LoadFailed,
	Ended
};

USTRUCT(BlueprintType)
struct NOPHOTOS_API FNPPlayerRanking
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Score")
	TObjectPtr<ANPPlayerState> PlayerState = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Score")
	int32 Score = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNPOnPlayerRankingsChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNPOnMainGameStateChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNPOnMainGameEnded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNPOnMainGameLastSpurt);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNPOnPictureSelectionStateChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNoPhotosPhotoEvidenceChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNPOnPhotoLikesChanged, FGuid, PhotoId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNPOnPhotoFullyLiked, FGuid, PhotoId, int32, LikeCount);

UCLASS()
class NOPHOTOS_API ANPMainGameState : public AGameState
{
	GENERATED_BODY()

public:
	virtual void RemovePlayerState(APlayerState* PlayerState) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Main Game")
	TArray<FNPPlayerRanking> GetPlayerRankings() const;

	UFUNCTION(BlueprintPure, Category = "Main Game")
	int32 GetRemainingGameTime() const;

	UFUNCTION(BlueprintPure, Category = "Main Game")
	bool IsMainGameActive() const;

	UFUNCTION(BlueprintPure, Category = "Main Game")
	bool IsMainGameEnded() const;

	UFUNCTION(BlueprintPure, Category = "Main Game|Loading")
	ENPMainWorldState GetMainWorldState() const { return MainWorldState; }

	UFUNCTION(BlueprintPure, Category = "Main Game|Loading")
	bool IsMainWorldReady() const { return MainWorldState == ENPMainWorldState::Playing; }

	UFUNCTION(BlueprintPure, Category = "Main Game|Loading")
	float GetCountdownEndServerTime() const { return CountdownEndServerTime; }

	UFUNCTION(BlueprintPure, Category = "Picture Selection")
	bool IsPlayerPictureSelectionComplete(const APlayerState* PlayerState) const;

	UFUNCTION(BlueprintPure, Category = "Photo")
	TArray<FNPReplicatedPhotoEvidence> GetPhotoEvidence() const { return PhotoEvidence; }

	UFUNCTION(BlueprintPure, Category = "Photo|Validation")
	int32 GetMinimumVisibleHeadSampleCount() const { return MinimumVisibleHeadSampleCount; }

	UFUNCTION(BlueprintPure, Category = "Photo")
	TArray<FGuid> GetTransferredPhotoIds() const { return TransferredPhotoIds; }

	UFUNCTION(BlueprintPure, Category = "Photo")
	TArray<FGuid> GetSelectedPhotoIds(const APlayerState* PlayerState) const;

	UFUNCTION(BlueprintPure, Category = "Photo|Like")
	int32 GetPhotoLikeCount(FGuid PhotoId) const;

	UFUNCTION(BlueprintPure, Category = "Photo|Like")
	int32 GetMaximumPhotoLikeCount() const;

	UFUNCTION(BlueprintPure, Category = "Photo|Like")
	bool HasPlayerLikedPhoto(FGuid PhotoId, const APlayerState* PlayerState) const;

	UFUNCTION(BlueprintPure, Category = "Photo|Like")
	bool CanPlayerLikePhoto(FGuid PhotoId, const APlayerState* PlayerState) const;

	//모든 플레이어가 사진 선택을 완료했으면 정산 화면으로 전환
	void ConfirmPictureSelection(APlayerController* PlayerController);

	UPROPERTY(BlueprintAssignable, Category = "Main Game")
	FNPOnPlayerRankingsChanged OnPlayerRankingsChanged;

	UPROPERTY(BlueprintAssignable, Category = "Main Game")
	FNPOnMainGameStateChanged OnMainGameStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Main Game")
	FNPOnMainGameEnded OnMainGameEnded;

	UPROPERTY(BlueprintAssignable, Category = "Main Game")
	FNPOnMainGameLastSpurt OnMainGameLastSpurt;

	UPROPERTY(BlueprintAssignable, Category = "Picture Selection")
	FNPOnPictureSelectionStateChanged OnPictureSelectionStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Photo")
	FOnNoPhotosPhotoEvidenceChanged OnPhotoEvidenceChanged;

	UPROPERTY(BlueprintAssignable, Category = "Photo|Like")
	FNPOnPhotoLikesChanged OnPhotoLikesChanged;

	UPROPERTY(BlueprintAssignable, Category = "Photo|Like")
	FNPOnPhotoFullyLiked OnPhotoFullyLiked;

	void RefreshPlayerRankings();
	void SetMainWorldState(ENPMainWorldState NewState);
	void BeginGameStartCountdown(float EndServerTime);
	void StartMainGame(int32 DurationSeconds);
	void SetRemainingGameTime(int32 RemainingSeconds);
	void FinishMainGame();
	void AddPhotoEvidence(const FNPPhotoEvidenceResult& Result, int32 AwardedScore);
	void SetMinimumVisibleHeadSampleCount(int32 InSampleCount);
	void RegisterTransferredPhoto(const FGuid& PhotoId);
	void SetSelectedPhotoIds(APlayerState* PlayerState, const TArray<FGuid>& PhotoIds);
	bool AddPhotoLike(const FGuid& PhotoId, APlayerState* PlayerState);

private:
	UFUNCTION()
	void OnRep_PlayerRankings();

	UFUNCTION()
	void OnRep_MainGameState();

	UFUNCTION()
	void OnRep_MainGameEnded();

	UFUNCTION()
	void OnRep_PictureSelectionCompletedPlayers();

	UFUNCTION()
	void OnRep_PhotoEvidence();

	UFUNCTION()
	void OnRep_TransferredPhotoIds();

	UFUNCTION()
	void OnRep_SelectedPhotos();

	UFUNCTION()
	void OnRep_PhotoLikes();

	UFUNCTION()
	void OnRep_ResultParticipants();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPhotoFullyLiked(FGuid PhotoId, int32 LikeCount);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastMainGameLastSpurt();

	bool AreAllConnectedPlayersPictureSelectionComplete() const;
	APlayerState* FindSelectedPhotoOwner(const FGuid& PhotoId) const;

	void LogLocalGameStatus();
	void TryLogFinalRankings();

	UPROPERTY(ReplicatedUsing = OnRep_PlayerRankings)
	TArray<FNPPlayerRanking> PlayerRankings;

	UPROPERTY(ReplicatedUsing = OnRep_MainGameState)
	int32 RemainingGameTime = 0;

	UPROPERTY(ReplicatedUsing = OnRep_MainGameState)
	bool bMainGameActive = false;

	UPROPERTY(ReplicatedUsing = OnRep_MainGameEnded)
	bool bMainGameEnded = false;

	UPROPERTY(ReplicatedUsing = OnRep_MainGameState)
	ENPMainWorldState MainWorldState = ENPMainWorldState::Preparing;

	UPROPERTY(ReplicatedUsing = OnRep_MainGameState)
	float CountdownEndServerTime = 0.0f;

	//사진 선택 완료를 누른 플레이어 목록
	UPROPERTY(ReplicatedUsing = OnRep_PictureSelectionCompletedPlayers)
	TArray<TObjectPtr<APlayerState>> PictureSelectionCompletedPlayers;

	static constexpr int32 MaximumStoredPhotosPerPlayer = 30;
	static constexpr int32 MaximumTransferredPhotos = 30;

	UPROPERTY(ReplicatedUsing = OnRep_PhotoEvidence)
	TArray<FNPReplicatedPhotoEvidence> PhotoEvidence;

	UPROPERTY(Replicated)
	int32 MinimumVisibleHeadSampleCount = 2;

	UPROPERTY(ReplicatedUsing = OnRep_TransferredPhotoIds)
	TArray<FGuid> TransferredPhotoIds;

	UPROPERTY(ReplicatedUsing = OnRep_SelectedPhotos)
	TArray<FNPPlayerSelectedPhotos> SelectedPhotos;

	/** 정산 화면 진입 시점의 참가자 목록입니다. */
	UPROPERTY(ReplicatedUsing = OnRep_ResultParticipants)
	TArray<TObjectPtr<APlayerState>> ResultParticipants;

	UPROPERTY(ReplicatedUsing = OnRep_PhotoLikes)
	TArray<FNPPhotoLikeState> PhotoLikes;

	int32 LastLoggedRemainingTime = INDEX_NONE;
	bool bFinalRankingsLogged = false;
	
	bool bFinalMinuteBGMStarted = false;
};
