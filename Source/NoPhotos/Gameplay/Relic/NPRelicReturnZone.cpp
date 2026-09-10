#include "Gameplay/Relic/NPRelicReturnZone.h"

#include "Components/BoxComponent.h"
#include "Gameplay/Relic/NPBaseRelic.h"
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
	}
}

void ANPRelicReturnZone::HandleReturnVolumeBeginOverlap(
	UPrimitiveComponent*,
	AActor* OtherActor,
	UPrimitiveComponent*,
	int32,
	bool,
	const FHitResult&)
{
	ANPBaseRelic* Relic = Cast<ANPBaseRelic>(OtherActor);
	ANPMainGameMode* GameMode = GetWorld()
		? GetWorld()->GetAuthGameMode<ANPMainGameMode>()
		: nullptr;
	UNPRelicDeliveryService* DeliveryService = GameMode
		? GameMode->GetRelicDeliveryService()
		: nullptr;
	if (Relic && DeliveryService)
	{
		const FVector DeliveryLocation = Relic->GetRelicWorldLocation();
		if (DeliveryService->TryDeliverRelic(Relic, this)
			&& bDeliveryEffectEnabled)
		{
			MulticastNotifyRelicDelivered(Relic, DeliveryLocation);
		}
	}
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
