#include "Gameplay/Relic/Gimmick/Ship/NPShipGimmickSequenceManager.h"

#include "Engine/World.h"
#include "Gameplay/Relic/Gimmick/Ship/NPShipGimmickBase.h"
#include "Net/UnrealNetwork.h"

ANPShipGimmickSequenceManager::ANPShipGimmickSequenceManager()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

void ANPShipGimmickSequenceManager::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	for (ANPShipGimmickBase* Gimmick : GimmickSequence)
	{
		if (IsValid(Gimmick))
		{
			Gimmick->OnActivated.AddDynamic(this, &ThisClass::HandleGimmickActivated);
		}
	}
}

void ANPShipGimmickSequenceManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		for (ANPShipGimmickBase* Gimmick : GimmickSequence)
		{
			if (IsValid(Gimmick))
			{
				Gimmick->OnActivated.RemoveDynamic(this, &ThisClass::HandleGimmickActivated);
			}
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ANPShipGimmickSequenceManager::HandleGimmickActivated(ANPShipGimmickBase* ActivatedGimmick)
{
	if (!HasAuthority() || bSequenceCompleted || !GimmickSequence.IsValidIndex(CurrentStep))
	{
		return;
	}

	if (ActivatedGimmick != GimmickSequence[CurrentStep])
	{
		ResetSequence();
		return;
	}

	++CurrentStep;
	OnProgressed.Broadcast(CurrentStep, GimmickSequence.Num());
	ReceiveSequenceProgressed(CurrentStep, GimmickSequence.Num());

	if (CurrentStep == GimmickSequence.Num())
	{
		bSequenceCompleted = true;
		SpawnSequenceTreasure();
		OnSequenceCompleted.Broadcast();
		ReceiveSequenceCompleted();
	}

	ForceNetUpdate();
}

void ANPShipGimmickSequenceManager::SpawnSequenceTreasure()
{
	if (!HasAuthority() || !TreasureClass)
	{
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	GetWorld()->SpawnActor<AActor>(
		TreasureClass,
		FTransform(GetActorRotation(), GetActorLocation() + TreasureSpawnOffset),
		SpawnParameters);
}

void ANPShipGimmickSequenceManager::ResetSequence()
{
	if (!HasAuthority() || bSequenceCompleted)
	{
		return;
	}

	CurrentStep = 0;
	for (ANPShipGimmickBase* Gimmick : GimmickSequence)
	{
		if (IsValid(Gimmick))
		{
			Gimmick->ResetShipGimmick();
		}
	}

	OnSequenceReset.Broadcast();
	ReceiveSequenceReset();
	ForceNetUpdate();
}

void ANPShipGimmickSequenceManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ANPShipGimmickSequenceManager, CurrentStep);
	DOREPLIFETIME(ANPShipGimmickSequenceManager, bSequenceCompleted);
}
