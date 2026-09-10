#include "Gameplay/Relic/Gimmick/Ship/NPShipGimmickGameplayCue.h"

ANPShipGimmickGameplayCue::ANPShipGimmickGameplayCue()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	bAutoAttachToOwner = false;
}

bool ANPShipGimmickGameplayCue::OnExecute_Implementation(
	AActor* Target,
	const FGameplayCueParameters& Parameters)
{
	Super::OnExecute_Implementation(Target, Parameters);

	const FVector CueLocation = Parameters.Location;
	const FVector CueNormal = FVector(Parameters.Normal).GetSafeNormal();
	if (CueNormal.IsNearlyZero())
	{
		SetActorLocation(CueLocation);
	}
	else
	{
		SetActorLocationAndRotation(CueLocation, CueNormal.Rotation());
	}
	
	GameplayCueFinishedCallback();
	return true;
}

