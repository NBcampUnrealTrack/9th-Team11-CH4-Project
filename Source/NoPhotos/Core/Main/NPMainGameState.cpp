#include "NPMainGameState.h"

#include "Core/Main/NPMainPlayerController.h"
#include "Core/NPPlayerState.h"
#include "Gameplay/Character/NPReplicatedStablePhysicsPawn.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Gameplay/MapEvents/NPMapEvent.h"
#include "Gameplay/MapEvents/NPMapEventManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "NPMainGameLog.h"

void ANPMainGameState::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ANPMainGameState, PlayerRankings);
	DOREPLIFETIME(ANPMainGameState, RemainingGameTime);
	DOREPLIFETIME(ANPMainGameState, bMainGameActive);
	DOREPLIFETIME(ANPMainGameState, bMainGameEnded);
	DOREPLIFETIME(ANPMainGameState, MainWorldState);
	DOREPLIFETIME(
		ANPMainGameState,
		PictureSelectionCompletedPlayers);
	DOREPLIFETIME(ANPMainGameState, PhotoEvidence);
	DOREPLIFETIME(ANPMainGameState, MinimumVisibleHeadSampleCount);
	DOREPLIFETIME(ANPMainGameState, TransferredPhotoIds);
	DOREPLIFETIME(ANPMainGameState, SelectedPhotos);
	DOREPLIFETIME(ANPMainGameState, ResultParticipants);
	DOREPLIFETIME(ANPMainGameState, PhotoLikes);
}

TArray<FNPPlayerRanking> ANPMainGameState::GetPlayerRankings() const
{
	return PlayerRankings;
}

int32 ANPMainGameState::GetRemainingGameTime() const
{
	return RemainingGameTime;
}

bool ANPMainGameState::IsMainGameActive() const
{
	return bMainGameActive;
}

bool ANPMainGameState::IsMainGameEnded() const
{
	return bMainGameEnded;
}

void ANPMainGameState::SetMainWorldState(const ENPMainWorldState NewState)
{
	if (!HasAuthority() || MainWorldState == NewState)
	{
		return;
	}

	MainWorldState = NewState;
	ForceNetUpdate();
	OnMainGameStateChanged.Broadcast();
}

bool ANPMainGameState::IsPlayerPictureSelectionComplete(const APlayerState* PlayerState) const
{
	return IsValid(PlayerState)
		&& PictureSelectionCompletedPlayers.Contains(
			const_cast<APlayerState*>(PlayerState));
}

TArray<FGuid> ANPMainGameState::GetSelectedPhotoIds(const APlayerState* PlayerState) const
{
	if (!IsValid(PlayerState))
	{
		return {};
	}

	for (const FNPPlayerSelectedPhotos& Selected : SelectedPhotos)
	{
		if (Selected.PlayerState == PlayerState)
		{
			return Selected.PhotoIds;
		}
	}

	return {};
}

int32 ANPMainGameState::GetPhotoLikeCount(const FGuid PhotoId) const
{
	for (const FNPPhotoLikeState& LikeState : PhotoLikes)
	{
		if (LikeState.PhotoId == PhotoId)
		{
			return LikeState.LikedPlayerStates.Num();
		}
	}

	return 0;
}

int32 ANPMainGameState::GetMaximumPhotoLikeCount() const
{
	return FMath::Max(0, ResultParticipants.Num() - 1);
}

bool ANPMainGameState::HasPlayerLikedPhoto(
	const FGuid PhotoId,
	const APlayerState* PlayerState) const
{
	if (!IsValid(PlayerState))
	{
		return false;
	}

	for (const FNPPhotoLikeState& LikeState : PhotoLikes)
	{
		if (LikeState.PhotoId == PhotoId)
		{
			return LikeState.LikedPlayerStates.Contains(
				const_cast<APlayerState*>(PlayerState));
		}
	}

	return false;
}

bool ANPMainGameState::CanPlayerLikePhoto(
	const FGuid PhotoId,
	const APlayerState* PlayerState) const
{
	const APlayerState* PhotoOwner = FindSelectedPhotoOwner(PhotoId);
	return PhotoId.IsValid()
		&& IsValid(PlayerState)
		&& IsValid(PhotoOwner)
		&& PhotoOwner != PlayerState
		&& ResultParticipants.Contains(const_cast<APlayerState*>(PlayerState))
		&& GetMaximumPhotoLikeCount() > 0
		&& !HasPlayerLikedPhoto(PhotoId, PlayerState)
		&& GetPhotoLikeCount(PhotoId) < GetMaximumPhotoLikeCount();
}

bool ANPMainGameState::AddPhotoLike(
	const FGuid& PhotoId,
	APlayerState* PlayerState)
{
	if (!HasAuthority() || !CanPlayerLikePhoto(PhotoId, PlayerState))
	{
		return false;
	}

	FNPPhotoLikeState* LikeState = PhotoLikes.FindByPredicate(
		[&PhotoId](const FNPPhotoLikeState& Entry)
		{
			return Entry.PhotoId == PhotoId;
		});
	if (!LikeState)
	{
		LikeState = &PhotoLikes.AddDefaulted_GetRef();
		LikeState->PhotoId = PhotoId;
	}

	LikeState->LikedPlayerStates.Add(PlayerState);
	const int32 LikeCount = LikeState->LikedPlayerStates.Num();
	ForceNetUpdate();
	OnPhotoLikesChanged.Broadcast(PhotoId);

	if (LikeCount == GetMaximumPhotoLikeCount())
	{
		MulticastPhotoFullyLiked(PhotoId, LikeCount);
	}

	return true;
}

void ANPMainGameState::SetSelectedPhotoIds(APlayerState* PlayerState, const TArray<FGuid>& PhotoIds)
{
	if (!HasAuthority() || !IsValid(PlayerState))
	{
		return;
	}

	for (FNPPlayerSelectedPhotos& Selected : SelectedPhotos)
	{
		if (Selected.PlayerState == PlayerState)
		{
			Selected.PhotoIds = PhotoIds;
			ForceNetUpdate();
			OnPhotoEvidenceChanged.Broadcast();
			return;
		}
	}

	FNPPlayerSelectedPhotos& NewSelected = SelectedPhotos.AddDefaulted_GetRef();
	NewSelected.PlayerState = PlayerState;
	NewSelected.PhotoIds = PhotoIds;
	ForceNetUpdate();
	OnPhotoEvidenceChanged.Broadcast();
}

void ANPMainGameState::AddPhotoEvidence(const FNPPhotoEvidenceResult& Result, const int32 AwardedScore)
{
	if (!HasAuthority() || !Result.bSuccess)
	{
		return;
	}

	FNPReplicatedPhotoEvidence& NewEvidence = PhotoEvidence.AddDefaulted_GetRef();
	NewEvidence.PhotoId = Result.PhotoId;
	NewEvidence.CaptureSequence = Result.CaptureSequence;
	NewEvidence.Photographer = Result.Photographer;
	for (const FNPPhotoRelicEvidenceGroup& EvidenceGroup : Result.RelicEvidenceGroups)
	{
		NewEvidence.Relics.AddUnique(EvidenceGroup.Relic);
		for (APlayerState* Thief : EvidenceGroup.Thieves)
		{
			NewEvidence.Thieves.AddUnique(Thief);
		}
	}
	NewEvidence.AwardedScore = AwardedScore;
	NewEvidence.ServerCaptureTime = Result.ServerCaptureTime;
	int32 PhotographerPhotoCount = 0;
	for (int32 EvidenceIndex = PhotoEvidence.Num() - 1; EvidenceIndex >= 0; --EvidenceIndex)
	{
		if (PhotoEvidence[EvidenceIndex].Photographer != Result.Photographer)
		{
			continue;
		}
		++PhotographerPhotoCount;
		if (PhotographerPhotoCount > MaximumStoredPhotosPerPlayer)
		{
			PhotoEvidence.RemoveAt(EvidenceIndex);
		}
	}
	ForceNetUpdate();
	OnPhotoEvidenceChanged.Broadcast();
}

void ANPMainGameState::SetMinimumVisibleHeadSampleCount(const int32 InSampleCount)
{
	if (!HasAuthority())
	{
		return;
	}

	MinimumVisibleHeadSampleCount = FMath::Clamp(InSampleCount, 1, 19);
	ForceNetUpdate();
}

void ANPMainGameState::RegisterTransferredPhoto(const FGuid& PhotoId)
{
	if (!HasAuthority() || !PhotoId.IsValid() || TransferredPhotoIds.Contains(PhotoId))
	{
		return;
	}

	TransferredPhotoIds.Add(PhotoId);
	if (TransferredPhotoIds.Num() > MaximumTransferredPhotos)
	{
		TransferredPhotoIds.RemoveAt(0, TransferredPhotoIds.Num() - MaximumTransferredPhotos);
	}
	ForceNetUpdate();
	OnPhotoEvidenceChanged.Broadcast();
}

void ANPMainGameState::ConfirmPictureSelection(APlayerController* PlayerController)
{
	if (!HasAuthority()
		|| !bMainGameEnded
		|| !IsValid(PlayerController)
		|| !IsValid(PlayerController->PlayerState))
	{
		return;
	}

	PictureSelectionCompletedPlayers.AddUnique(
		PlayerController->PlayerState);

	ForceNetUpdate();
	OnPictureSelectionStateChanged.Broadcast();

	if (!AreAllConnectedPlayersPictureSelectionComplete())
	{
		return;
	}

	ResultParticipants.Reset();
	for (FConstPlayerControllerIterator Iterator =
		GetWorld()->GetPlayerControllerIterator();
		Iterator;
		++Iterator)
	{
		const APlayerController* ConnectedPlayerController = Iterator->Get();
		if (IsValid(ConnectedPlayerController)
			&& IsValid(ConnectedPlayerController->PlayerState))
		{
			ResultParticipants.AddUnique(ConnectedPlayerController->PlayerState);
		}
	}
	PhotoLikes.Empty();
	ForceNetUpdate();

	for (FConstPlayerControllerIterator Iterator =
		GetWorld()->GetPlayerControllerIterator();
		Iterator;
		++Iterator)
	{
		ANPMainPlayerController* MainPlayerController =
			Cast<ANPMainPlayerController>(Iterator->Get());

		if (IsValid(MainPlayerController))
		{
			MainPlayerController->ClientShowResultUI();
		}
	}
}

void ANPMainGameState::RemovePlayerState(APlayerState* PlayerState)
{
	if (HasAuthority() && IsValid(PlayerState))
	{
		if (ANPReplicatedStablePhysicsPawn* Pawn = Cast<ANPReplicatedStablePhysicsPawn>(PlayerState->GetPawn()))
		{
			Pawn->SetRankingLeader(false);
		}
	}
	Super::RemovePlayerState(PlayerState);
	RefreshPlayerRankings();
}

void ANPMainGameState::RefreshPlayerRankings()
{
	if (!HasAuthority())
	{
		return;
	}

	PlayerRankings.Reset();

	for (APlayerState* PlayerState : PlayerArray)
	{
		ANPPlayerState* NPPlayerState =
			Cast<ANPPlayerState>(PlayerState);

		if (!NPPlayerState)
		{
			continue;
		}

		FNPPlayerRanking& Ranking =
			PlayerRankings.AddDefaulted_GetRef();

		Ranking.PlayerState = NPPlayerState;
		Ranking.Score = NPPlayerState->GetPlayerScore();
	}

	PlayerRankings.Sort(
		[](const FNPPlayerRanking& Left, const FNPPlayerRanking& Right)
		{
			if (Left.Score != Right.Score)
			{
				return Left.Score > Right.Score;
			}

			const int32 LeftPlayerId = Left.PlayerState ? Left.PlayerState->GetPlayerId() : INDEX_NONE;
			const int32 RightPlayerId = Right.PlayerState ? Right.PlayerState->GetPlayerId() : INDEX_NONE;
			return LeftPlayerId < RightPlayerId;
		});

	const int32 HighestScore = PlayerRankings.IsEmpty() ? 0 : PlayerRankings[0].Score;
	for (const FNPPlayerRanking& Ranking : PlayerRankings)
	{
		if (ANPReplicatedStablePhysicsPawn* Pawn = Cast<ANPReplicatedStablePhysicsPawn>(Ranking.PlayerState->GetPawn()))
		{
			Pawn->SetRankingLeader(bMainGameActive && HighestScore > 0 && Ranking.Score == HighestScore);
		}
	}

	ForceNetUpdate();
	OnPlayerRankingsChanged.Broadcast();
}

void ANPMainGameState::StartMainGame(const int32 DurationSeconds)
{
	if (!HasAuthority())
	{
		return;
	}

	RemainingGameTime = FMath::Max(1, DurationSeconds);
	bMainGameActive = true;
	bMainGameEnded = false;
	MainWorldState = ENPMainWorldState::Playing;

	//새게임 시작시 이전게임 완료상태 초기화
	PictureSelectionCompletedPlayers.Empty();
	ResultParticipants.Empty();
	PhotoLikes.Empty();

	LastLoggedRemainingTime = INDEX_NONE;
	bFinalRankingsLogged = false;

	RefreshPlayerRankings();
	ForceNetUpdate();

	OnMainGameStateChanged.Broadcast();
	OnPictureSelectionStateChanged.Broadcast();

	LogLocalGameStatus();
}

void ANPMainGameState::SetRemainingGameTime(const int32 RemainingSeconds)
{
	if (!HasAuthority() || !bMainGameActive)
	{
		return;
	}

	RemainingGameTime = FMath::Max(0, RemainingSeconds);
	ForceNetUpdate();
	OnMainGameStateChanged.Broadcast();
	if (!bFinalMinuteBGMStarted && RemainingGameTime <= 69 && MainWorldState == ENPMainWorldState::Playing)
	{
		if (RemainingGameTime <= 65)
			bFinalMinuteBGMStarted = true;
		MulticastMainGameLastSpurt();
	}
	LogLocalGameStatus();
}

void ANPMainGameState::MulticastMainGameLastSpurt_Implementation()
{
	OnMainGameLastSpurt.Broadcast();
}

void ANPMainGameState::FinishMainGame()
{
	if (!HasAuthority() || bMainGameEnded)
	{
		return;
	}

	RemainingGameTime = 0;
	bMainGameActive = false;
	bMainGameEnded = true;
	MainWorldState = ENPMainWorldState::Ended;

	TInlineComponentArray<UNPMapEventManagerComponent*> EventManagers(this);
	for (UNPMapEventManagerComponent* EventManager : EventManagers)
	{
		if (IsValid(EventManager))
		{
			EventManager->ShutdownEventsForGameEnd();
		}
	}
	// 매니저 카탈로그 외에 레벨에 직접 배치하거나 수동 생성한 이벤트도 종료합니다.
	for (TActorIterator<ANPMapEvent> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && It->IsEventActive())
		{
			It->FinishEvent();
		}
	}

	//사진선택 시작 전 완료목록 비우기
	PictureSelectionCompletedPlayers.Empty();

	RefreshPlayerRankings();
	ForceNetUpdate();
	OnMainGameStateChanged.Broadcast();
	OnMainGameEnded.Broadcast();
	OnPictureSelectionStateChanged.Broadcast();
	TryLogFinalRankings();

	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator;	++Iterator)
	{
		ANPMainPlayerController* MainPlayerController = Cast<ANPMainPlayerController>(Iterator->Get());
		if (IsValid(MainPlayerController))
		{
			MainPlayerController->ClientShowSelectPictureUI();
		}
	}
}

void ANPMainGameState::OnRep_PlayerRankings()
{
	OnPlayerRankingsChanged.Broadcast();
	TryLogFinalRankings();
}

void ANPMainGameState::OnRep_MainGameState()
{
	OnMainGameStateChanged.Broadcast();
	if (bMainGameActive)
	{
		LogLocalGameStatus();
		return;
	}

	TryLogFinalRankings();
}

void ANPMainGameState::OnRep_MainGameEnded()
{
	if (bMainGameEnded)
	{
		OnMainGameEnded.Broadcast();
	}
}

void ANPMainGameState::OnRep_PictureSelectionCompletedPlayers()
{
	OnPictureSelectionStateChanged.Broadcast();
}

void ANPMainGameState::OnRep_PhotoEvidence()
{
	OnPhotoEvidenceChanged.Broadcast();
}

void ANPMainGameState::OnRep_TransferredPhotoIds()
{
	OnPhotoEvidenceChanged.Broadcast();
}

void ANPMainGameState::OnRep_SelectedPhotos()
{
	OnPhotoEvidenceChanged.Broadcast();
}

void ANPMainGameState::OnRep_PhotoLikes()
{
	OnPhotoLikesChanged.Broadcast(FGuid());
}

void ANPMainGameState::OnRep_ResultParticipants()
{
	OnPhotoLikesChanged.Broadcast(FGuid());
}

void ANPMainGameState::MulticastPhotoFullyLiked_Implementation(
	const FGuid PhotoId,
	const int32 LikeCount)
{
	OnPhotoFullyLiked.Broadcast(PhotoId, LikeCount);
}

bool ANPMainGameState::AreAllConnectedPlayersPictureSelectionComplete() const
{
	bool bHasConnectedPlayer = false;

	for (FConstPlayerControllerIterator Iterator =
		GetWorld()->GetPlayerControllerIterator();
		Iterator;
		++Iterator)
	{
		const APlayerController* PlayerController =
			Iterator->Get();

		if (!IsValid(PlayerController)
			|| !IsValid(PlayerController->PlayerState))
		{
			continue;
		}

		bHasConnectedPlayer = true;

		if (!PictureSelectionCompletedPlayers.Contains(
			PlayerController->PlayerState))
		{
			return false;
		}
	}

	return bHasConnectedPlayer;
}

APlayerState* ANPMainGameState::FindSelectedPhotoOwner(const FGuid& PhotoId) const
{
	if (!PhotoId.IsValid())
	{
		return nullptr;
	}

	for (const FNPPlayerSelectedPhotos& Selected : SelectedPhotos)
	{
		if (IsValid(Selected.PlayerState) && Selected.PhotoIds.Contains(PhotoId))
		{
			return Selected.PlayerState;
		}
	}

	return nullptr;
}

void ANPMainGameState::LogLocalGameStatus()
{
	if (!bMainGameActive || LastLoggedRemainingTime == RemainingGameTime)
	{
		return;
	}

	APlayerController* LocalPlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const ANPPlayerState* LocalPlayerState = LocalPlayerController
		? LocalPlayerController->GetPlayerState<ANPPlayerState>()
		: nullptr;
	if (!LocalPlayerState || !LocalPlayerController->IsLocalController())
	{
		return;
	}

	LastLoggedRemainingTime = RemainingGameTime;
	NPMainGameLog::Info(
		this,
		FString::Printf(
			TEXT("남은 시간=%d초, 내 점수=%d점"),
			RemainingGameTime,
			LocalPlayerState->GetPlayerScore()));
}

void ANPMainGameState::TryLogFinalRankings()
{
	if (!bMainGameEnded	|| bFinalRankingsLogged	|| PlayerRankings.IsEmpty())
	{
		return;
	}

	APlayerController* LocalPlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!LocalPlayerController || !LocalPlayerController->IsLocalController())
	{
		return;
	}

	bFinalRankingsLogged = true;
	NPMainGameLog::Info(this, TEXT("게임 종료 - 최종 순위"));
	for (int32 RankingIndex = 0; RankingIndex < PlayerRankings.Num(); ++RankingIndex)
	{
		const FNPPlayerRanking& Ranking = PlayerRankings[RankingIndex];
		const FString PlayerName = Ranking.PlayerState
			? Ranking.PlayerState->GetPlayerName()
			: TEXT("Unknown");
		NPMainGameLog::Info(
			this,
			FString::Printf(
				TEXT("%d위 - %s: %d점"),
				RankingIndex + 1,
				*PlayerName,
				Ranking.Score));
	}
}
