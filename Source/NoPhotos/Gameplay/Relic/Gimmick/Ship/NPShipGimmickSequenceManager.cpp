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
	if (!HasAuthority() || bSequenceCompleted || !IsValid(ActivatedGimmick))
	{
		return;
	}

	if (!GimmickSequence.Contains(ActivatedGimmick))
	{
		return;
	}

	if (CompletedGimmicks.Contains(ActivatedGimmick))
	{
		return;
	}

	CompletedGimmicks.Add(ActivatedGimmick);
	CurrentStep = CompletedGimmicks.Num();
	OnProgressed.Broadcast(CurrentStep, GimmickSequence.Num());
	ReceiveSequenceProgressed(CurrentStep, GimmickSequence.Num());

	if (CurrentStep >= GimmickSequence.Num())
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
	CompletedGimmicks.Empty();
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
