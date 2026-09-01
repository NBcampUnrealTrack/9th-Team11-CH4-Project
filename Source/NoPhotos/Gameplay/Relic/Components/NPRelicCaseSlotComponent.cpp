#include "Gameplay/Relic/Components/NPRelicCaseSlotComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Gameplay/Relic/NPBaseRelic.h"
#include "Net/UnrealNetwork.h"

UNPRelicCaseSlotComponent::UNPRelicCaseSlotComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UNPRelicCaseSlotComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UNPRelicCaseSlotComponent, SpawnedRelic);
	DOREPLIFETIME(UNPRelicCaseSlotComponent, bIsRelicReleased);
}

ANPBaseRelic* UNPRelicCaseSlotComponent::SpawnRelic(
	const bool bInitiallyReleased)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return SpawnedRelic;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	bIsRelicReleased = bInitiallyReleased;
	if (!IsValid(SpawnedRelic))
	{
		if (!RelicClass)
		{
			return nullptr;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.OverrideLevel = Owner->GetLevel();
		SpawnParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		const FTransform SpawnTransform(
			GetComponentQuat(),
			GetComponentLocation(),
			FVector::OneVector);
		SpawnedRelic = World->SpawnActor<ANPBaseRelic>(
			RelicClass,
			SpawnTransform,
			SpawnParameters);
	}

	if (!IsValid(SpawnedRelic))
	{
		return nullptr;
	}

	SpawnedRelic->SetUnlocked(bIsRelicReleased);
	ApplyRelicState();
	Owner->ForceNetUpdate();
	return SpawnedRelic;
}

#if WITH_EDITOR
ANPBaseRelic* UNPRelicCaseSlotComponent::RecreateRelicInEditor()
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World || World->IsGameWorld())
	{
		return SpawnedRelic;
	}

	Modify();
	if (IsValid(SpawnedRelic))
	{
		SpawnedRelic->Modify();
		if (!World->EditorDestroyActor(SpawnedRelic, true))
		{
			return SpawnedRelic;
		}
		SpawnedRelic = nullptr;
	}

	bIsRelicReleased = false;
	if (RelicClass)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.OverrideLevel = Owner->GetLevel();
		SpawnParameters.ObjectFlags |= RF_Transactional;
		SpawnParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		const FTransform SpawnTransform(
			GetComponentQuat(),
			GetComponentLocation(),
			FVector::OneVector);
		SpawnedRelic = World->SpawnActor<ANPBaseRelic>(
			RelicClass,
			SpawnTransform,
			SpawnParameters);
		if (IsValid(SpawnedRelic))
		{
			SpawnedRelic->SetUnlocked(false);
			ApplyRelicState();
		}
	}

	Owner->MarkPackageDirty();
	return SpawnedRelic;
}
#endif

void UNPRelicCaseSlotComponent::ReleaseRelic()
{
	SetCaseAccessible(true);
}

void UNPRelicCaseSlotComponent::SetCaseAccessible(const bool bAccessible)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || bIsRelicReleased == bAccessible)
	{
		return;
	}
	if (IsValid(SpawnedRelic) && (!SpawnedRelic->IsDisplayed() || SpawnedRelic->IsReturned()))
	{
		return;
	}

	Owner->FlushNetDormancy();
	bIsRelicReleased = bAccessible;
	if (IsValid(SpawnedRelic))
	{
		SpawnedRelic->SetUnlocked(bAccessible);
	}
	ApplyRelicState();
	Owner->ForceNetUpdate();
}

void UNPRelicCaseSlotComponent::OnRep_SpawnedRelic()
{
	ApplyRelicState();
}

void UNPRelicCaseSlotComponent::OnRep_IsRelicReleased()
{
	ApplyRelicState();
}

void UNPRelicCaseSlotComponent::ApplyRelicState()
{
	if (!IsValid(SpawnedRelic))
	{
		return;
	}

	if (UPrimitiveComponent* RelicPrimitive =
		Cast<UPrimitiveComponent>(SpawnedRelic->GetRootComponent()))
	{
		RelicPrimitive->SetCollisionResponseToChannel(
			ECC_Destructible,
			ECR_Ignore);
	}
	SpawnedRelic->SetActorEnableCollision(bIsRelicReleased);
}
