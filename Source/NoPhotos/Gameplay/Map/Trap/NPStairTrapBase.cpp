#include "Gameplay/Map/Trap/NPStairTrapBase.h"

#include "Components/SceneComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

ANPStairTrapBase::ANPStairTrapBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	TrapRootComponent = CreateDefaultSubobject<USceneComponent>(
		TEXT("TrapRootComponent"));
	SetRootComponent(TrapRootComponent);
}

void ANPStairTrapBase::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANPStairTrapBase, ReplicatedTrapState);
}

float ANPStairTrapBase::GetTrapPhaseElapsedTime() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}

	const AGameStateBase* GameState = World->GetGameState();
	const float ServerTime = GameState
		? GameState->GetServerWorldTimeSeconds()
		: World->GetTimeSeconds();
	return FMath::Max(
		0.0f,
		ServerTime - ReplicatedTrapState.PhaseStartServerTime);
}

void ANPStairTrapBase::SetTrapEnabled(const bool bEnabled)
{
	if (!HasAuthority())
	{
		return;
	}

	ApplyControlledState(
		bEnabled ? ENPStairTrapState::Idle : ENPStairTrapState::Disabled,
		GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f,
		ReplicatedTrapState.CycleSequence);
}

void ANPStairTrapBase::HandleTrapStateChanged(
	const ENPStairTrapState,
	const ENPStairTrapState)
{
}

void ANPStairTrapBase::ApplyControlledState(
	const ENPStairTrapState NewState,
	const float PhaseStartServerTime,
	const int32 CycleSequence)
{
	if (!HasAuthority())
	{
		return;
	}

	const ENPStairTrapState PreviousState = ReplicatedTrapState.State;
	ReplicatedTrapState.State = NewState;
	ReplicatedTrapState.PhaseStartServerTime = PhaseStartServerTime;
	ReplicatedTrapState.CycleSequence = CycleSequence;
	NotifyTrapStateChanged(PreviousState);
	ForceNetUpdate();
}

void ANPStairTrapBase::OnRep_TrapState(
	const FNPStairTrapRepState PreviousState)
{
	NotifyTrapStateChanged(PreviousState.State);
}

void ANPStairTrapBase::NotifyTrapStateChanged(
	const ENPStairTrapState PreviousState)
{
	HandleTrapStateChanged(PreviousState, ReplicatedTrapState.State);
	BP_OnTrapStateChanged(
		PreviousState,
		ReplicatedTrapState.State,
		ReplicatedTrapState.CycleSequence,
		ReplicatedTrapState.PhaseStartServerTime);
}
