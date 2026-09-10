#include "Gameplay/Relic/NPRelicReturnZone.h"

#include "Components/BoxComponent.h"
#include "Gameplay/Relic/NPBaseRelic.h"
#include "Gameplay/Relic/Components/NPRelicOwnershipComponent.h"
#include "Gameplay/Relic/NPRelicDeliveryEffect.h"
#include "Gameplay/Relic/NPRelicDeliveryService.h"
#include "Core/Main/NPMainGameMode.h"
#include "Core/NPPlayerState.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

ANPRelicReturnZone::ANPRelicReturnZone()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

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
		return false;
	}

	DeliveryAttemptsInProgress.Add(Relic);
	const int32 RelicPrice = Relic->GetCurrentPrice();
	const UStaticMeshComponent* RelicMeshComponent =
		Cast<UStaticMeshComponent>(Relic->GetRootComponent());
	UStaticMesh* RelicMesh = RelicMeshComponent
		? RelicMeshComponent->GetStaticMesh()
		: nullptr;
	const FTransform DeliveryTransform = RelicMeshComponent
		? RelicMeshComponent->GetComponentTransform()
		: Relic->GetActorTransform();

	TArray<AActor*> DeliveryTargets;
	if (UNPRelicOwnershipComponent* Ownership = Relic->GetOwnershipComponent())
	{
		TArray<ANPPlayerState*> Owners;
		Ownership->GetCurrentOwners(Owners);
		for (const ANPPlayerState* OwnerPlayerState : Owners)
		{
			APawn* OwnerPawn = OwnerPlayerState
				? OwnerPlayerState->GetPawn()
				: nullptr;
			if (!IsValid(OwnerPawn))
			{
				continue;
			}

			DeliveryTargets.AddUnique(OwnerPawn);
		}
	}

	const bool bDelivered = DeliveryService->TryDeliverRelic(Relic, this);
	DeliveryAttemptsInProgress.Remove(Relic);
	if (!bDelivered)
	{
		return false;
	}

	const bool bHasDeliverySound = LowPriceSound
		|| MidPriceSound
		|| LargePriceSound;
	if ((DeliveryEffectClass && IsValid(RelicMesh))
		|| bDeliveryEffectEnabled
		|| bHasDeliverySound)
	{
		MulticastNotifyRelicDelivered(
			Relic,
			DeliveryTransform,
			RelicMesh,
			DeliveryTargets,
			RelicPrice,
			bDeliveryEffectEnabled);
	}

	UnregisterOverlappingRelic(Relic);
	return true;
}

void ANPRelicReturnZone::MulticastNotifyRelicDelivered_Implementation(
	ANPBaseRelic* DeliveredRelic,
	const FTransform& DeliveryTransform,
	UStaticMesh* RelicMesh,
	const TArray<AActor*>& DeliveryTargets,
	const int32 RelicPrice,
	const bool bNotifyBlueprint)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	USoundBase* DeliverySound = nullptr;
	if (RelicPrice <= LowPriceThreshold)
	{
		DeliverySound = LowPriceSound;
	}
	else if (RelicPrice <= MidPriceThreshold)
	{
		DeliverySound = MidPriceSound;
	}
	else
	{
		DeliverySound = LargePriceSound;
	}

	if (DeliverySound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			DeliverySound,
			DeliveryTransform.GetLocation());
	}

	if (DeliveryEffectClass && IsValid(RelicMesh))
	{
		for (AActor* DeliveryTarget : DeliveryTargets)
		{
			if (!IsValid(DeliveryTarget))
			{
				continue;
			}

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.Owner = this;
			SpawnParameters.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (ANPRelicDeliveryEffect* DeliveryEffect =
				GetWorld()->SpawnActor<ANPRelicDeliveryEffect>(
					DeliveryEffectClass,
					DeliveryTransform,
					SpawnParameters))
			{
				DeliveryEffect->InitializeEffect(
					RelicMesh,
					DeliveryTarget,
					RelicPrice);
			}
		}
	}

	if (bNotifyBlueprint)
	{
		BP_OnRelicDelivered(
			DeliveredRelic,
			DeliveryTransform.GetLocation());
	}
}
