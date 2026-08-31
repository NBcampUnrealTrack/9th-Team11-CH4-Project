#include "NPSpeedBoostMapEvent.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/Character/Component/NPStablePhysicsMovementComponent.h"
#include "Gameplay/Character/NPStablePhysicsPawn.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogNPSpeedBoostEvent, Log, All);

ANPSpeedBoostMapEvent::ANPSpeedBoostMapEvent()
{
	Duration = 10.0f;
	SpeedMultiplier = 2.0f;
}

void ANPSpeedBoostMapEvent::ApplyEventState_Implementation(const bool bNewActive)
{
	if (!HasAuthority())
	{
		return;
	}

	if (bNewActive)
	{
		RefreshAffectedPlayers();
		GetWorldTimerManager().SetTimer(
			PlayerRefreshTimer,
			this,
			&ANPSpeedBoostMapEvent::RefreshAffectedPlayers,
			0.5f,
			true);

		UE_LOG(
			LogNPSpeedBoostEvent,
			Display,
			TEXT("플레이어 이동속도 증가 이벤트 시작: Multiplier=%.2f, Duration=%.2f"),
			SpeedMultiplier,
			GetEventDuration());
		return;
	}

	GetWorldTimerManager().ClearTimer(PlayerRefreshTimer);
	RestoreAffectedPlayers();
	UE_LOG(LogNPSpeedBoostEvent, Display, TEXT("플레이어 이동속도 증가 이벤트 종료: Multiplier=1.00"));
}

void ANPSpeedBoostMapEvent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		GetWorldTimerManager().ClearTimer(PlayerRefreshTimer);
		RestoreAffectedPlayers();
	}

	Super::EndPlay(EndPlayReason);
}

void ANPSpeedBoostMapEvent::RefreshAffectedPlayers()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World || !IsEventActive())
	{
		return;
	}

	const float BoostedMoveSpeed = FMath::Max(0.0f, NormalMoveSpeed) * FMath::Max(1.0f, SpeedMultiplier);
	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PlayerController = Iterator->Get();
		ANPStablePhysicsPawn* Pawn = PlayerController
			? Cast<ANPStablePhysicsPawn>(PlayerController->GetPawn())
			: nullptr;
		if (!IsValid(Pawn))
		{
			continue;
		}

		ApplySpeed(Pawn, BoostedMoveSpeed);
		AffectedPawns.Add(Pawn);
	}
}

void ANPSpeedBoostMapEvent::RestoreAffectedPlayers()
{
	for (const TWeakObjectPtr<ANPStablePhysicsPawn>& PawnPtr : AffectedPawns)
	{
		ApplySpeed(PawnPtr.Get(), FMath::Max(0.0f, NormalMoveSpeed));
	}

	// 마지막 갱신 직후 리스폰한 플레이어도 종료 시 반드시 원상복구합니다.
	if (UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			APlayerController* PlayerController = Iterator->Get();
			ApplySpeed(
				PlayerController ? Cast<ANPStablePhysicsPawn>(PlayerController->GetPawn()) : nullptr,
				FMath::Max(0.0f, NormalMoveSpeed));
		}
	}

	AffectedPawns.Reset();
}

void ANPSpeedBoostMapEvent::ApplySpeed(
	ANPStablePhysicsPawn* Pawn,
	const float MoveSpeed)
{
	if (!IsValid(Pawn))
	{
		return;
	}

	if (UNPStablePhysicsMovementComponent* Movement = Pawn->GetStablePhysicsMovementComponent())
	{
		Movement->SetMaxMoveSpeed(MoveSpeed);
	}
}
