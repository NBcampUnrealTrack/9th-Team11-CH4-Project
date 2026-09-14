#include "Gameplay/Map/Trap/NPStairTrapController.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Gameplay/Map/Trap/NPStairTrapBase.h"
#include "NoPhotos.h"
#include "TimerManager.h"

ANPStairTrapController::ANPStairTrapController()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
}

void ANPStairTrapController::PostLoad()
{
	Super::PostLoad();
	MigrateLegacyTrapEntries();
}

void ANPStairTrapController::BeginPlay()
{
	Super::BeginPlay();
	MigrateLegacyTrapEntries();

	if (!HasAuthority())
	{
		return;
	}

	SetAllTrapStates(ENPStairTrapState::Idle);
	if (bStartAutomatically)
	{
		StartTrapSequence();
	}
}

void ANPStairTrapController::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	ClearScheduledTransitions();
	Super::EndPlay(EndPlayReason);
}

void ANPStairTrapController::StartTrapSequence()
{
	if (!HasAuthority() || bSequenceRunning)
	{
		return;
	}

	bSequenceRunning = true;
	++CurrentRunGeneration;
	bContinuousTrapsStarted = false;
	if (InitialDelay > 0.0f)
	{
		ScheduleTransition(
			FTimerDelegate::CreateUObject(
				this,
				&ThisClass::BeginWarningPhase),
			InitialDelay);
		return;
	}

	BeginWarningPhase();
}

void ANPStairTrapController::StopTrapSequence()
{
	if (!HasAuthority())
	{
		return;
	}

	ClearScheduledTransitions();
	bSequenceRunning = false;
	++CurrentRunGeneration;
	bContinuousTrapsStarted = false;
	SetAllTrapStates(ENPStairTrapState::Idle);
	BP_OnTrapSequenceStopped();
}

void ANPStairTrapController::BeginWarningPhase()
{
	if (!bSequenceRunning)
	{
		return;
	}

	// 만료된 핸들만 제거하여 아직 시작 대기 중인 Continuous 함정의
	// 타이머는 StopTrapSequence에서 취소할 수 있게 유지합니다.
	TrapTransitionTimers.RemoveAll(
		[this](const FTimerHandle& TimerHandle)
		{
			return !GetWorldTimerManager().TimerExists(TimerHandle);
		});
	++CurrentCycleSequence;
	BP_OnTrapCycleStarted(CurrentCycleSequence);

	float MaximumStartDelay = 0.0f;
	bool bHasCyclicTrap = false;
	for (int32 TrapIndex = 0;
		TrapIndex < ControlledTrapEntries.Num();
		++TrapIndex)
	{
		const FNPStairTrapControlEntry& Entry =
			ControlledTrapEntries[TrapIndex];
		ANPStairTrapBase* Trap = Entry.Trap;
		if (!IsValid(Trap))
		{
			continue;
		}

		const float StartDelay = FMath::Max(0.0f, Entry.StartDelay);
		if (Trap->GetTrapOperationMode()
			== ENPStairTrapOperationMode::Continuous)
		{
			if (!bContinuousTrapsStarted)
			{
				ScheduleTrapState(
					TrapIndex,
					ENPStairTrapState::Active,
					StartDelay,
					CurrentCycleSequence,
					CurrentRunGeneration);
			}
			continue;
		}

		bHasCyclicTrap = true;
		MaximumStartDelay = FMath::Max(MaximumStartDelay, StartDelay);

		ScheduleTrapState(
			TrapIndex,
			ENPStairTrapState::Warning,
			StartDelay,
			CurrentCycleSequence,
			CurrentRunGeneration);
		ScheduleTrapState(
			TrapIndex,
			ENPStairTrapState::Active,
			StartDelay + WarningDuration,
			CurrentCycleSequence,
			CurrentRunGeneration);
		ScheduleTrapState(
			TrapIndex,
			ENPStairTrapState::Returning,
			StartDelay + WarningDuration + ActiveDuration,
			CurrentCycleSequence,
			CurrentRunGeneration);
		ScheduleTrapState(
			TrapIndex,
			ENPStairTrapState::Cooldown,
			StartDelay + WarningDuration + ActiveDuration
				+ ReturningDuration,
			CurrentCycleSequence,
			CurrentRunGeneration);
	}
	bContinuousTrapsStarted = true;

	// Continuous 함정만 있다면 명시적으로 정지할 때까지 실행합니다.
	if (!bHasCyclicTrap)
	{
		return;
	}

	const float CycleDuration = MaximumStartDelay
		+ WarningDuration
		+ ActiveDuration
		+ ReturningDuration
		+ CooldownDuration;
	ScheduleTransition(
		FTimerDelegate::CreateUObject(
			this,
			&ThisClass::FinishCycle),
		CycleDuration);
}

void ANPStairTrapController::FinishCycle()
{
	if (!bSequenceRunning)
	{
		return;
	}

	if (bLoopSequence)
	{
		BeginWarningPhase();
		return;
	}

	bSequenceRunning = false;
	SetAllTrapStates(ENPStairTrapState::Idle);
}

void ANPStairTrapController::ApplyTrapState(
	const int32 TrapIndex,
	const ENPStairTrapState NewState,
	const int32 CycleSequence,
	const int32 RunGeneration)
{
	if (!bSequenceRunning || RunGeneration != CurrentRunGeneration
		|| !ControlledTrapEntries.IsValidIndex(TrapIndex))
	{
		return;
	}

	ANPStairTrapBase* Trap = ControlledTrapEntries[TrapIndex].Trap;
	if (!IsValid(Trap))
	{
		return;
	}

	Trap->ApplyControlledState(
		NewState,
		GetServerTimeSeconds(),
		CycleSequence);
}

void ANPStairTrapController::ScheduleTrapState(
	const int32 TrapIndex,
	const ENPStairTrapState NewState,
	const float Delay,
	const int32 CycleSequence,
	const int32 RunGeneration)
{
	if (Delay <= 0.0f)
	{
		ApplyTrapState(
			TrapIndex,
			NewState,
			CycleSequence,
			RunGeneration);
		return;
	}

	FTimerHandle& TimerHandle = TrapTransitionTimers.AddDefaulted_GetRef();
	FTimerDelegate Transition;
	Transition.BindUObject(
		this,
		&ThisClass::ApplyTrapState,
		TrapIndex,
		NewState,
		CycleSequence,
		RunGeneration);
	GetWorldTimerManager().SetTimer(
		TimerHandle,
		MoveTemp(Transition),
		Delay,
		false);
}

void ANPStairTrapController::SetAllTrapStates(
	const ENPStairTrapState NewState)
{
	const float PhaseStartServerTime = GetServerTimeSeconds();
	for (const FNPStairTrapControlEntry& Entry : ControlledTrapEntries)
	{
		ANPStairTrapBase* Trap = Entry.Trap;
		if (!IsValid(Trap))
		{
			continue;
		}

		Trap->ApplyControlledState(
			NewState,
			PhaseStartServerTime,
			CurrentCycleSequence);
	}
}

void ANPStairTrapController::ScheduleTransition(
	FTimerDelegate Transition,
	const float Delay)
{
	if (Delay <= 0.0f)
	{
		GetWorldTimerManager().SetTimerForNextTick(MoveTemp(Transition));
		return;
	}

	GetWorldTimerManager().SetTimer(
		PhaseTimer,
		MoveTemp(Transition),
		Delay,
		false);
}

void ANPStairTrapController::ClearScheduledTransitions()
{
	GetWorldTimerManager().ClearTimer(PhaseTimer);
	for (FTimerHandle& TimerHandle : TrapTransitionTimers)
	{
		GetWorldTimerManager().ClearTimer(TimerHandle);
	}
	TrapTransitionTimers.Reset();
}

void ANPStairTrapController::MigrateLegacyTrapEntries()
{
	if (!ControlledTrapEntries.IsEmpty() || ControlledTraps.IsEmpty())
	{
		return;
	}

	ControlledTrapEntries.Reserve(ControlledTraps.Num());
	for (int32 TrapIndex = 0; TrapIndex < ControlledTraps.Num(); ++TrapIndex)
	{
		FNPStairTrapControlEntry& Entry =
			ControlledTrapEntries.AddDefaulted_GetRef();
		Entry.Trap = ControlledTraps[TrapIndex];
		Entry.StartDelay = TrapStartDelays.IsValidIndex(TrapIndex)
			? FMath::Max(0.0f, TrapStartDelays[TrapIndex])
			: 0.0f;
	}
}

float ANPStairTrapController::GetServerTimeSeconds() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}

	const AGameStateBase* GameState = World->GetGameState();
	return GameState
		? GameState->GetServerWorldTimeSeconds()
		: World->GetTimeSeconds();
}
