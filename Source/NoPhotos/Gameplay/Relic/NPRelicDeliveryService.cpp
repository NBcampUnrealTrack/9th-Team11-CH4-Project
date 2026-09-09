#include "Gameplay/Relic/NPRelicDeliveryService.h"

#include "Core/Main/NPMainGameState.h"
#include "Core/NPPlayerState.h"
#include "Gameplay/Photo/NPPhotoEvidenceTypes.h"
#include "Gameplay/Relic/NPBaseRelic.h"
#include "Gameplay/Relic/Components/NPRelicOwnershipComponent.h"
#include "Gameplay/Relic/NPRelicReturnZone.h"
#include "Gameplay/Character/NPReplicatedStablePhysicsPawn.h"
#include "NoPhotos.h"
#include "Core/Main/NPMainGameMode.h"
#include "UI/GameScreen/NPScoreFeedbackWidgetComponent.h"

void UNPRelicDeliveryService::Initialize(ANPMainGameMode* InGameMode)
{
	OwningGameMode = InGameMode;
}

UWorld* UNPRelicDeliveryService::GetWorld() const
{
	return OwningGameMode.IsValid() ? OwningGameMode->GetWorld() : nullptr;
}

bool UNPRelicDeliveryService::RegisterPhotoEvidence(const FNPPhotoEvidenceResult& Evidence)
{
	if (!OwningGameMode.IsValid() || !OwningGameMode->HasAuthority() || !Evidence.bSuccess)
	{
		return false;
	}

	ANPBaseRelic* Relic = Cast<ANPBaseRelic>(Evidence.Relic);
	if (!Relic || !IsValid(Evidence.Photographer))
	{
		return false;
	}

	const bool bPenaltyChanged = Relic->AddPhotoPenaltyCapture(
		PhotoPenaltyRatePerCapture);
	UE_LOG(
		LogNoPhotos,
		Log,
		TEXT("[RelicDelivery] Evidence penalty %s. Relic=%s Photographer=%s CaptureCount=%d PenaltyRate=%.2f AccumulatedPenalty=%d ReturnScore=%d"),
		bPenaltyChanged ? TEXT("applied") : TEXT("clamped/ignored"),
		*GetNameSafe(Relic),
		*GetNameSafe(Evidence.Photographer),
		Relic->GetSuccessfulEvidenceCaptureCount(),
		PhotoPenaltyRatePerCapture,
		Relic->GetAccumulatedPhotoPenalty(),
		CalculateReturnScore(Relic));
	return bPenaltyChanged;
}

int32 UNPRelicDeliveryService::CalculateReturnScore(const ANPBaseRelic* Relic) const
{
	if (!IsValid(Relic))
	{
		return 0;
	}

	return Relic->GetCurrentPrice();
}

bool UNPRelicDeliveryService::TryDeliverRelic(
	ANPBaseRelic* Relic,
	ANPRelicReturnZone* ReturnZone)
{
	if (!OwningGameMode.IsValid() || !OwningGameMode->HasAuthority()
		|| !IsValid(Relic) || !IsValid(ReturnZone) || Relic->IsReturned())
	{
		return false;
	}

	ANPMainGameState* MainGameState = GetWorld()
		? GetWorld()->GetGameState<ANPMainGameState>()
		: nullptr;
	UNPRelicOwnershipComponent* Ownership = Relic->GetOwnershipComponent();
	TArray<ANPPlayerState*> Owners;
	if (Ownership)
	{
		Ownership->GetCurrentOwners(Owners);
	}
	Owners.RemoveAll([](const ANPPlayerState* Owner)
	{
		return !IsValid(Owner);
	});
	Owners.Sort([](const ANPPlayerState& Left, const ANPPlayerState& Right)
	{
		return Left.GetPlayerId() < Right.GetPlayerId();
	});

	if (!MainGameState || !MainGameState->IsMainGameActive() || Owners.IsEmpty())
	{
		UE_LOG(
			LogNoPhotos,
			Warning,
			TEXT("[RelicDelivery] Delivery rejected. Relic=%s GameActive=%s OwnerCount=%d"),
			*GetNameSafe(Relic),
			MainGameState && MainGameState->IsMainGameActive() ? TEXT("true") : TEXT("false"),
			Owners.Num());
		return false;
	}

	const int32 BaseReturnScore = CalculateReturnScore(Relic);
	const double ScoreMultiplier = FMath::Max(
		0.0,
		static_cast<double>(ReturnZone->GetReturnScoreMultiplier()));
	const int32 AwardedScore = static_cast<int32>(FMath::Clamp<int64>(
		FMath::RoundToInt64(static_cast<double>(BaseReturnScore) * ScoreMultiplier),
		0,
		MAX_int32));
	const int32 ScorePerOwner = AwardedScore / Owners.Num();
	const int32 ScoreRemainder = AwardedScore % Owners.Num();
	Ownership->ReleaseAllGrabbers();
	if (!Relic->TryMarkReturned())
	{
		return false;
	}

	for (int32 OwnerIndex = 0; OwnerIndex < Owners.Num(); ++OwnerIndex)
	{
		const int32 OwnerScore = ScorePerOwner + (OwnerIndex < ScoreRemainder ? 1 : 0);
		if (OwnerScore > 0)
		{
			Owners[OwnerIndex]->AddScore(OwnerScore);

			ANPReplicatedStablePhysicsPawn* OwnerPawn =
				Cast<ANPReplicatedStablePhysicsPawn>(
					Owners[OwnerIndex]->GetPawn());
			if (UNPScoreFeedbackWidgetComponent* ScoreFeedback = OwnerPawn
				? OwnerPawn->FindComponentByClass<UNPScoreFeedbackWidgetComponent>()
				: nullptr)
			{
				ScoreFeedback->ShowScoreFeedback(
					OwnerScore,
					ENPScoreFeedbackType::RelicReturnReward,
					2.0f);
			}
		}
	}
	Ownership->ClearOwnership();
	UE_LOG(
		LogNoPhotos,
		Log,
		TEXT("[RelicDelivery] Relic returned. Relic=%s OwnerCount=%d BasePrice=%d PhotoPenalty=%d ReturnScore=%d ZoneMultiplier=%.2f TotalScore=%d"),
		*GetNameSafe(Relic),
		Owners.Num(),
		Relic->GetBasePrice(),
		Relic->GetAccumulatedPhotoPenalty(),
		BaseReturnScore,
		ScoreMultiplier,
		AwardedScore);
	return true;
}
