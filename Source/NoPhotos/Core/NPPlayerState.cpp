#include "NPPlayerState.h"

#include "Main/NPMainGameState.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

void ANPPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ANPPlayerState, PlayerScore);
}

void ANPPlayerState::AddScore(const int32 ScoreToAdd)
{
	if (!HasAuthority())
	{
		return;
	}

	ANPMainGameState* MainGameState = GetWorld() ? GetWorld()->GetGameState<ANPMainGameState>() : nullptr;
	if (!MainGameState || !MainGameState->IsMainGameActive())
	{
		return;
	}

	PlayerScore += ScoreToAdd;
	ForceNetUpdate();
	OnPlayerScoreChanged.Broadcast();
	MainGameState->RefreshPlayerRankings();
}

int32 ANPPlayerState::GetPlayerScore() const
{
	return PlayerScore;
}

void ANPPlayerState::ResetPlayerScore()
{
	if (!HasAuthority() || PlayerScore == 0)
	{
		return;
	}

	PlayerScore = 0;
	ForceNetUpdate();
	OnPlayerScoreChanged.Broadcast();
}

void ANPPlayerState::OnRep_PlayerScore()
{
	OnPlayerScoreChanged.Broadcast();
}
