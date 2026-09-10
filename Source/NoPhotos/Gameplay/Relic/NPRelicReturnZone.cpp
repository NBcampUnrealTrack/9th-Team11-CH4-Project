#include "Gameplay/Relic/NPRelicReturnZone.h"

#include "Components/BoxComponent.h"
#include "Gameplay/Relic/NPBaseRelic.h"
#include "Gameplay/Relic/Components/NPRelicOwnershipComponent.h"
#include "Gameplay/Relic/NPRelicDeliveryService.h"
#include "Core/Main/NPMainGameMode.h"

ANPRelicReturnZone::ANPRelicReturnZone()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	ReturnVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("ReturnVolume"));
	SetRootComponent(ReturnVolume);
	ReturnVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ReturnVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	ReturnVolume->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	ReturnVolume->SetGenerateOverlapEvents(true);
}

void ANPRelicReturnZone::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		ReturnVolume->OnComponentBeginOverlap.AddDynamic(
			this,
			&ANPRelicReturnZone::HandleReturnVolumeBeginOverlap);
		ReturnVolume->OnComponentEndOverlap.AddDynamic(
			this,
			&ANPRelicReturnZone::HandleReturnVolumeEndOverlap);

		TArray<AActor*> InitiallyOverlappingActors;
		ReturnVolume->GetOverlappingActors(
			InitiallyOverlappingActors,
			ANPBaseRelic::StaticClass());
		for (AActor* OverlappingActor : InitiallyOverlappingActors)
		{
			RegisterOverlappingRelic(Cast<ANPBaseRelic>(OverlappingActor));
		}
	}
}

void ANPRelicReturnZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	TArray<TWeakObjectPtr<ANPBaseRelic>> RelicsToUnregister =
		OverlappingRelics.Array();
	for (const TWeakObjectPtr<ANPBaseRelic>& Relic : RelicsToUnregister)
	{
		UnregisterOverlappingRelic(Relic.Get());
	}
	OverlappingRelics.Reset();
	DeliveryAttemptsInProgress.Reset();

	Super::EndPlay(EndPlayReason);
}

void ANPRelicReturnZone::HandleReturnVolumeBeginOverlap(
	UPrimitiveComponent*,
	AActor* OtherActor,
	UPrimitiveComponent*,
	int32,
	bool,
	const FHitResult&)
{
	RegisterOverlappingRelic(Cast<ANPBaseRelic>(OtherActor));
}

void ANPRelicReturnZone::HandleReturnVolumeEndOverlap(
	UPrimitiveComponent*,
	AActor* OtherActor,
	UPrimitiveComponent*,
	int32)
{
	ANPBaseRelic* Relic = Cast<ANPBaseRelic>(OtherActor);
	if (Relic && !ReturnVolume->IsOverlappingActor(Relic))
	{
		UnregisterOverlappingRelic(Relic);
	}
}

void ANPRelicReturnZone::RegisterOverlappingRelic(ANPBaseRelic* Relic)
{
	if (!HasAuthority() || !IsValid(Relic) || Relic->IsReturned())
	{
		return;
	}

	if (!OverlappingRelics.Contains(Relic))
	{
		OverlappingRelics.Add(Relic);
		if (UNPRelicOwnershipComponent* Ownership =
			Relic->GetOwnershipComponent())
		{
			Ownership->OnOwnershipChanged.AddUObject(
				this,
				&ThisClass::HandleRelicOwnershipChanged);
		}
	}

	TryDeliverOverlappingRelic(Relic);
}

void ANPRelicReturnZone::UnregisterOverlappingRelic(ANPBaseRelic* Relic)
{
	if (!IsValid(Relic))
	{
		return;
	}

	if (UNPRelicOwnershipComponent* Ownership =
		Relic->GetOwnershipComponent())
	{
		Ownership->OnOwnershipChanged.RemoveAll(this);
	}
	OverlappingRelics.Remove(Relic);
	DeliveryAttemptsInProgress.Remove(Relic);
}

void ANPRelicReturnZone::HandleRelicOwnershipChanged(
	UNPRelicOwnershipComponent* Ownership)
{
	ANPBaseRelic* Relic = Ownership
		? Cast<ANPBaseRelic>(Ownership->GetOwner())
		: nullptr;
	if (Relic && OverlappingRelics.Contains(Relic)
		&& ReturnVolume->IsOverlappingActor(Relic))
	{
		TryDeliverOverlappingRelic(Relic);
	}
}

bool ANPRelicReturnZone::TryDeliverOverlappingRelic(ANPBaseRelic* Relic)
{
	if (!HasAuthority() || !IsValid(Relic) || Relic->IsReturned()
		|| DeliveryAttemptsInProgress.Contains(Relic))
	{
		return false;
	}

	ANPMainGameMode* GameMode = GetWorld()
		? GetWorld()->GetAuthGameMode<ANPMainGameMode>()
		: nullptr;
	UNPRelicDeliveryService* DeliveryService = GameMode
		? GameMode->GetRelicDeliveryService()
		: nullptr;
	if (!DeliveryService)
	{
		const FVector DeliveryLocation = Relic->GetRelicWorldLocation();
		if (DeliveryService->TryDeliverRelic(Relic, this)
			&& bDeliveryEffectEnabled)
		{
			MulticastNotifyRelicDelivered(Relic, DeliveryLocation);
		}
		return false;
	}

	DeliveryAttemptsInProgress.Add(Relic);
	const FVector DeliveryLocation = Relic->GetRelicWorldLocation();
	const bool bDelivered = DeliveryService->TryDeliverRelic(Relic, this);
	DeliveryAttemptsInProgress.Remove(Relic);
	if (!bDelivered)
	{
		return false;
	}

	if (bDeliveryEffectEnabled)
	{
		MulticastNotifyRelicDelivered(DeliveryLocation);
	}
	UnregisterOverlappingRelic(Relic);
	return true;
}

void ANPRelicReturnZone::MulticastNotifyRelicDelivered_Implementation(
	ANPBaseRelic* DeliveredRelic,
	const FVector_NetQuantize10 DeliveryLocation)
{
	if (GetNetMode() != NM_DedicatedServer)
	{
		BP_OnRelicDelivered(DeliveredRelic, DeliveryLocation);
	}
}
