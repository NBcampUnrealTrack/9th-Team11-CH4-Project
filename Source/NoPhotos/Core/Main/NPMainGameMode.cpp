#include "NPMainGameMode.h"

#include "Core/Main/NPMainPlayerController.h"
#include "Core/NPPlayerState.h"
#include "Core/Room/NPRoomSubsystem.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "Gameplay/Photo/NPPhotoEvidenceService.h"
#include "Gameplay/Photo/NPPhotoLog.h"
#include "Gameplay/Photo/NPPhotoRepository.h"
#include "Gameplay/Character/NPReplicatedStablePhysicsPawn.h"
#include "Gameplay/Relic/NPRelicDeliveryService.h"
#include "NPMainGameLog.h"
#include "NPMainGameState.h"
#include "TimerManager.h"

ANPMainGameMode::ANPMainGameMode()
{
	GameStateClass = ANPMainGameState::StaticClass();
	PlayerControllerClass = ANPMainPlayerController::StaticClass();
	PlayerStateClass = ANPPlayerState::StaticClass();
	bUseSeamlessTravel = true;
	PhotoEvidenceServiceClass = UNPPhotoEvidenceService::StaticClass();
}

void ANPMainGameMode::InitGame(
	const FString& MapName,
	const FString& Options,
	FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	UClass* EvidenceClass = PhotoEvidenceServiceClass
		? PhotoEvidenceServiceClass.Get()
		: UNPPhotoEvidenceService::StaticClass();
	PhotoEvidenceService = NewObject<UNPPhotoEvidenceService>(this, EvidenceClass, TEXT("PhotoEvidenceService"));
	PhotoEvidenceService->Initialize(this);

	PhotoRepository = NewObject<UNPPhotoRepository>(this, TEXT("PhotoRepository"));
	PhotoRepository->Initialize(this);

	RelicDeliveryService = NewObject<UNPRelicDeliveryService>(this, TEXT("RelicDeliveryService"));
	RelicDeliveryService->Initialize(this);
}

FNPPhotoEvidenceResult ANPMainGameMode::HandlePhotoCaptureRequest(const FNPPhotoCaptureRequest& Request)
{
	FNPPhotoEvidenceResult Result;
	Result.CaptureSequence = Request.CaptureSequence;
	if (!HasAuthority() || !PhotoEvidenceService)
	{
		Result.FailureReason = ENPPhotoEvidenceFailureReason::InvalidPhotographer;
		return Result;
	}

	Result = PhotoEvidenceService->EvaluatePhoto(Request);
	PlayPhotoWorldFeedback(Result);
	if (PhotoRepository
		&& (Result.bSuccess
			|| Result.bReactiveTargetSuccess
			|| Result.FailureReason == ENPPhotoEvidenceFailureReason::NoValidEvidence))
	{
		PhotoRepository->AuthorizeCapture(Request.Photographer, Request.CaptureSequence);
	}
	if (!Result.bSuccess && !Result.bReactiveTargetSuccess)
	{
		UE_LOG(LogNPPhoto, Warning, TEXT("[GameMode] Photo evidence rejected. Reason=%d"),
			static_cast<int32>(Result.FailureReason));
		return Result;
	}

	if (Result.bSuccess && RelicDeliveryService)
	{
		RelicDeliveryService->RegisterPhotoEvidence(Result);
	}
	if (Result.bSuccess)
	{
		if (ANPMainGameState* MainGameState = GetGameState<ANPMainGameState>())
		{
			MainGameState->AddPhotoEvidence(Result, 0);
		}
	}

	UE_LOG(
		LogNPPhoto,
		Log,
		TEXT("[GameMode] Photo accepted. Photographer=%s PlayerCaptured=%s CapturedPlayer=%s RelicEvidence=%s Thief=%s Relic=%s ReactiveTarget=%s"),
		*GetNameSafe(Result.Photographer.Get()),
		Result.bPlayerCaptured ? TEXT("true") : TEXT("false"),
		*GetNameSafe(Result.CapturedPlayer.Get()),
		Result.bSuccess ? TEXT("true") : TEXT("false"),
		*GetNameSafe(Result.Thief.Get()),
		*GetNameSafe(Result.Relic.Get()),
		*GetNameSafe(Result.ReactiveTarget.Get()));
	return Result;
}

void ANPMainGameMode::PlayPhotoWorldFeedback(
	const FNPPhotoEvidenceResult& Result)
{
	if (!HasAuthority() || !Result.bPlayerCaptured)
	{
		return;
	}

	ANPReplicatedStablePhysicsPawn* PhotographerPawn = IsValid(Result.Photographer.Get())
		? Cast<ANPReplicatedStablePhysicsPawn>(Result.Photographer->GetPawn())
		: nullptr;
	ANPReplicatedStablePhysicsPawn* PhotographedPawn = IsValid(Result.CapturedPlayer.Get())
		? Cast<ANPReplicatedStablePhysicsPawn>(Result.CapturedPlayer->GetPawn())
		: nullptr;

	if (IsValid(PhotographerPawn))
	{
		PhotographerPawn->MulticastPlayPhotographerFeedback();
	}
	if (IsValid(PhotographedPawn))
	{
		PhotographedPawn->MulticastPlayPhotographedFeedback();
		if (ANPMainPlayerController* PhotographedController =
			Cast<ANPMainPlayerController>(PhotographedPawn->GetController()))
		{
			PhotographedController->ClientPlayPhotographedFlash();
		}
	}

	UE_LOG(
		LogNPPhoto,
		Log,
		TEXT("[PhotoWorldFeedback] Multicast requested. PhotographerPawn=%s PhotographedPawn=%s"),
		*GetNameSafe(PhotographerPawn),
		*GetNameSafe(PhotographedPawn));
}

void ANPMainGameMode::HandlePhotoStored(
	APlayerController* Photographer,
	const uint16 CaptureSequence,
	const FGuid& PhotoId)
{
	if (ANPMainGameState* MainGameState = GetGameState<ANPMainGameState>())
	{
		MainGameState->RegisterTransferredPhoto(PhotoId);
		MainGameState->AttachPhotoId(
			Photographer ? Photographer->PlayerState : nullptr,
			CaptureSequence,
			PhotoId);
	}
}

void ANPMainGameMode::BeginPlay()
{
	Super::BeginPlay();
	StartMainGame();
}

void ANPMainGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (ANPMainPlayerController* NPPlayerController = Cast<ANPMainPlayerController>(NewPlayer))
	{
		NPPlayerController->ClientShowGameScreenUI();
	}

	RefreshPlayerRankings();
}

void ANPMainGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);
	RefreshPlayerRankings();

	ANPMainGameState* MainGameState = GetGameState<ANPMainGameState>();
	if (GetNetMode() != NM_ListenServer || !MainGameState || !MainGameState->IsMainGameActive())
	{
		return;
	}

	int32 RemainingPlayerCount = 0;
	bool bHostRemains = false;
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		const APlayerController* PlayerController = Iterator->Get();
		if (!IsValid(PlayerController) || PlayerController == Exiting)
		{
			continue;
		}

		++RemainingPlayerCount;
		bHostRemains |= PlayerController->IsLocalController();
	}

	if (RemainingPlayerCount == 1 && bHostRemains)
	{
		GetWorldTimerManager().ClearTimer(MainGameTimer);
		MainGameState->FinishMainGame();
		EndMatch();
	}
}

void ANPMainGameMode::HandleSeamlessTravelPlayer(AController*& Controller)
{
	Super::HandleSeamlessTravelPlayer(Controller);

	if (ANPMainPlayerController* NPPlayerController = Cast<ANPMainPlayerController>(Controller))
	{
		NPPlayerController->ClientShowGameScreenUI();
	}

	RefreshPlayerRankings();
}

AActor* ANPMainGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	TSet<const AActor*> AssignedStartSpots;
	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		const APlayerController* PlayerController = Iterator->Get();
		if (IsValid(PlayerController) && PlayerController != Player && PlayerController->StartSpot.IsValid())
		{
			AssignedStartSpots.Add(PlayerController->StartSpot.Get());
		}
	}

	TArray<APlayerStart*> AvailableStartSpots;
	TArray<APlayerStart*> BlockedStartSpots;
	TArray<APlayerStart*> AssignedStarts;
	UClass* PawnClass = GetDefaultPawnClassForController(Player);
	APawn* PawnDefaults = PawnClass ? PawnClass->GetDefaultObject<APawn>() : nullptr;
	for (TActorIterator<APlayerStart> Iterator(World); Iterator; ++Iterator)
	{
		APlayerStart* PlayerStart = *Iterator;
		if (IsValid(PlayerStart) && AssignedStartSpots.Contains(PlayerStart))
		{
			AssignedStarts.Add(PlayerStart);
		}
		if (IsValid(PlayerStart) && !AssignedStartSpots.Contains(PlayerStart))
		{
			if (PawnDefaults && World->EncroachingBlockingGeometry(
				PawnDefaults, PlayerStart->GetActorLocation(), PlayerStart->GetActorRotation()))
			{
				BlockedStartSpots.Add(PlayerStart);
			}
			else
			{
				AvailableStartSpots.Add(PlayerStart);
			}
		}
	}

	if (AvailableStartSpots.IsEmpty())
	{
		if (!BlockedStartSpots.IsEmpty())
		{
			// SpawnDefaultPawnAtTransform performs a bounded search around this unassigned start.
			return BlockedStartSpots[FMath::RandHelper(BlockedStartSpots.Num())];
		}
		if (!AssignedStarts.IsEmpty())
		{
			// Small test maps may contain only one start. Search around it instead of spawning on another pawn.
			return AssignedStarts[FMath::RandHelper(AssignedStarts.Num())];
		}
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	return AvailableStartSpots[FMath::RandHelper(AvailableStartSpots.Num())];
}

APawn* ANPMainGameMode::SpawnDefaultPawnAtTransform_Implementation(
	AController* NewPlayer, const FTransform& SpawnTransform)
{
	UWorld* World = GetWorld();
	UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer);
	APawn* PawnDefaults = PawnClass ? PawnClass->GetDefaultObject<APawn>() : nullptr;
	if (!World || !PawnDefaults)
	{
		return nullptr;
	}

	// Skeletal physics pawns do not always resolve floor intersections with FindTeleportSpot.
	// Check nearby positions explicitly; never force a pawn into blocking geometry.
	const FVector Offsets[] = {
		FVector::ZeroVector, FVector(150, 0, 0), FVector(-150, 0, 0),
		FVector(0, 150, 0), FVector(0, -150, 0), FVector(150, 150, 0),
		FVector(150, -150, 0), FVector(-150, 150, 0), FVector(-150, -150, 0)
	};
	for (const float Height : {0.0f, 50.0f, 100.0f, 200.0f})
	{
		for (const FVector& Offset : Offsets)
		{
			FTransform Candidate = SpawnTransform;
			Candidate.AddToTranslation(Offset + FVector::UpVector * Height);
			if (World->EncroachingBlockingGeometry(PawnDefaults, Candidate.GetLocation(), Candidate.Rotator()))
			{
				continue;
			}
			if (APawn* Pawn = Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, Candidate))
			{
				return Pawn;
			}
		}
	}
	NPMainGameLog::Info(this, TEXT("플레이어 생성 실패: PlayerStart 주변 150cm/높이 200cm 이내에 빈 공간이 없습니다. 배치와 Pawn 충돌을 확인하세요."));
	return nullptr;
}

void ANPMainGameMode::RequestRestartRoom(APlayerController* RequestingPlayer)
{
	if (bReturningToRoom)
	{
		NPMainGameLog::Info(this, TEXT("대기방 복귀 요청 무시: 이미 복귀 처리 중입니다."));
		return;
	}

	if (GetNetMode() != NM_ListenServer || !IsValid(RequestingPlayer) || !RequestingPlayer->IsLocalController())
	{
		NPMainGameLog::Info(this, TEXT("대기방 복귀 거절: 리슨 서버 호스트만 요청할 수 있습니다."));
		return;
	}

	const ANPMainGameState* MainGameState = GetGameState<ANPMainGameState>();
	if (!MainGameState || !MainGameState->IsMainGameEnded())
	{
		NPMainGameLog::Info(this, TEXT("대기방 복귀 거절: 게임이 아직 종료되지 않았습니다."));
		return;
	}

	if (RoomLevel.IsNull())
	{
		NPMainGameLog::Info(this, TEXT("대기방 복귀 실패: RoomLevel이 지정되지 않았습니다."));
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UNPRoomSubsystem* RoomSubsystem = GameInstance ? GameInstance->GetSubsystem<UNPRoomSubsystem>() : nullptr;
	if (!RoomSubsystem)
	{
		NPMainGameLog::Info(this, TEXT("대기방 복귀 실패: RoomSubsystem을 찾지 못했습니다."));
		return;
	}

	bReturningToRoom = true;
	NPMainGameLog::Info(this, TEXT("호스트 대기방 복귀 요청 승인: 온라인 방 상태를 복구합니다."));
	RoomSubsystem->RestoreWaitingRoom(
		FNPOnWaitingRoomRestored::CreateUObject(this, &ANPMainGameMode::HandleWaitingRoomRestored));
}

void ANPMainGameMode::StartMainGame()
{
	ANPMainGameState* MainGameState = GetGameState<ANPMainGameState>();
	if (!MainGameState)
	{
		return;
	}

	MainGameState->StartMainGame(GameDurationSeconds);
	NPMainGameLog::Info(
		this,
		FString::Printf(TEXT("게임 시작: 제한시간=%d초"), GameDurationSeconds));
	GetWorldTimerManager().SetTimer(
		MainGameTimer,
		this,
		&ANPMainGameMode::UpdateMainGameTimer,
		1.0f,
		true);
}

void ANPMainGameMode::UpdateMainGameTimer()
{
	ANPMainGameState* MainGameState = GetGameState<ANPMainGameState>();
	if (!MainGameState)
	{
		GetWorldTimerManager().ClearTimer(MainGameTimer);
		return;
	}

	const int32 NextRemainingTime = MainGameState->GetRemainingGameTime() - 1;
	MainGameState->SetRemainingGameTime(NextRemainingTime);
	if (NextRemainingTime > 0)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(MainGameTimer);
	MainGameState->FinishMainGame();
	EndMatch();
}

void ANPMainGameMode::RefreshPlayerRankings()
{
	if (ANPMainGameState* MainGameState = GetGameState<ANPMainGameState>())
	{
		MainGameState->RefreshPlayerRankings();
	}
}

void ANPMainGameMode::HandleWaitingRoomRestored(const bool bWasSuccessful)
{
	if (!bWasSuccessful)
	{
		bReturningToRoom = false;
		NPMainGameLog::Info(this, TEXT("대기방 복귀 중단: 온라인 방 상태 복구에 실패했습니다."));
		return;
	}

	const FString RoomLevelPath = RoomLevel.ToSoftObjectPath().GetLongPackageName();
	NPMainGameLog::Info(this, FString::Printf(TEXT("대기방 복귀: ServerTravel -> %s"), *RoomLevelPath));
	GetWorld()->ServerTravel(RoomLevelPath);
}
