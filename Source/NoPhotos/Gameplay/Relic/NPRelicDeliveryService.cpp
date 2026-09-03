#include "Gameplay/Relic/NPRelicDeliveryService.h"

#include "Core/Main/NPMainGameState.h"
#include "Core/NPPlayerState.h"
#include "Gameplay/Photo/NPPhotoEvidenceTypes.h"
#include "Gameplay/Relic/NPBaseRelic.h"
#include "Gameplay/Relic/Components/NPRelicOwnershipComponent.h"
#include "Gameplay/Relic/NPRelicReturnZone.h"
#include "NoPhotos.h"
#include "Core/Main/NPMainGameMode.h"

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

	return FMath::Max(
		0,
		Relic->GetBasePrice() - Relic->GetAccumulatedPhotoPenalty());
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

	const int32 AwardedScore = CalculateReturnScore(Relic);
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
		}
	}
	Ownership->ClearOwnership();
	UE_LOG(
		LogNoPhotos,
		Log,
		TEXT("[RelicDelivery] Relic returned. Relic=%s OwnerCount=%d BasePrice=%d PhotoPenalty=%d TotalScore=%d"),
		*GetNameSafe(Relic),
		Owners.Num(),
		Relic->GetBasePrice(),
		Relic->GetAccumulatedPhotoPenalty(),
		AwardedScore);
	return true;
}
