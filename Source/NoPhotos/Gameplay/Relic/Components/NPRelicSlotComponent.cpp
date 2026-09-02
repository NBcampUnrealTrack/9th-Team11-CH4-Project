#include "Gameplay/Relic/Components/NPRelicSlotComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Data/Structs/NPRelicData.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Gameplay/Relic/NPBaseRelic.h"
#include "Net/UnrealNetwork.h"

UNPRelicSlotComponent::UNPRelicSlotComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UNPRelicSlotComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UNPRelicSlotComponent, SpawnedRelic);
	DOREPLIFETIME(UNPRelicSlotComponent, bIsRelicReleased);
}

void UNPRelicSlotComponent::CreateRelicInEditor()
{
#if WITH_EDITOR
	RecreateRelicInEditor();
#endif
}

ANPBaseRelic* UNPRelicSlotComponent::SpawnRelic(
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

	bIsRelicReleased = bInitiallyReleased
		|| bInitiallyAccessible
		|| bSimulatePhysicsOnSpawn;
	if (!IsValid(SpawnedRelic))
	{
		SpawnedRelic = CreateConfiguredRelic(RF_NoFlags);
	}

	if (!IsValid(SpawnedRelic))
	{
		return nullptr;
	}

	const bool bShouldSimulatePhysics = bSimulatePhysicsOnSpawn
		|| SpawnedRelic->ShouldStartWithPhysicsEnabled();
	bIsRelicReleased = bIsRelicReleased || bShouldSimulatePhysics;
	SpawnedRelic->SetUnlocked(bIsRelicReleased);
	ApplyRelicState();
	if (bShouldSimulatePhysics)
	{
		SpawnedRelic->ReleaseWithVelocityImpulse(FVector::ZeroVector);
	}
	Owner->ForceNetUpdate();
	return SpawnedRelic;
}

#if WITH_EDITOR
ANPBaseRelic* UNPRelicSlotComponent::RecreateRelicInEditor()
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

	bIsRelicReleased = bInitiallyAccessible || bSimulatePhysicsOnSpawn;
	SpawnedRelic = CreateConfiguredRelic(RF_Transactional);
	if (IsValid(SpawnedRelic))
	{
		const bool bShouldSimulatePhysics = bSimulatePhysicsOnSpawn
			|| SpawnedRelic->ShouldStartWithPhysicsEnabled();
		bIsRelicReleased = bIsRelicReleased || bShouldSimulatePhysics;
		SpawnedRelic->SetUnlocked(bIsRelicReleased);
		ApplyRelicState();
		if (bShouldSimulatePhysics)
		{
			SpawnedRelic->ReleaseWithVelocityImpulse(FVector::ZeroVector);
		}
	}

	Owner->MarkPackageDirty();
	return SpawnedRelic;
}
#endif

ANPBaseRelic* UNPRelicSlotComponent::CreateConfiguredRelic(
	const EObjectFlags InObjectFlags)
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World || !RelicData.DataTable || RelicData.RowName.IsNone())
	{
		return nullptr;
	}

	const FNPRelicTableRow* TableRow =
		RelicData.GetRow<FNPRelicTableRow>(TEXT("CreateConfiguredRelic"));
	UClass* RelicClass = TableRow
		? TableRow->RelicClass.LoadSynchronous()
		: nullptr;
	if (!RelicClass || RelicClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.OverrideLevel = Owner->GetLevel();
	SpawnParameters.ObjectFlags |= InObjectFlags;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParameters.bDeferConstruction = true;

	const FTransform SpawnTransform(
		GetComponentQuat(),
		GetComponentLocation(),
		FVector::OneVector);
	ANPBaseRelic* Relic = World->SpawnActor<ANPBaseRelic>(
		RelicClass,
		SpawnTransform,
		SpawnParameters);
	if (!IsValid(Relic))
	{
		return nullptr;
	}

	Relic->SetRelicTableData(RelicData);
	Relic->FinishSpawning(SpawnTransform);
	return Relic;
}

void UNPRelicSlotComponent::ReleaseRelic()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || bIsRelicReleased)
	{
		return;
	}

	bIsRelicReleased = true;
	if (IsValid(SpawnedRelic))
	{
		SpawnedRelic->SetUnlocked(true);
	}
	ApplyRelicState();
	Owner->ForceNetUpdate();
}

void UNPRelicSlotComponent::SetCaseAccessible(const bool bAccessible)
{
	bCaseAccessible = bAccessible;
	if (!IsValid(SpawnedRelic))
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (Owner && Owner->HasAuthority())
	{
		if (!bIsRelicReleased && !SpawnedRelic->IsDisplayed())
		{
			bIsRelicReleased = true;
			Owner->ForceNetUpdate();
		}

		SpawnedRelic->SetUnlocked(
			bInitiallyAccessible
			|| bIsRelicReleased
			|| bCaseAccessible);
	}

	ApplyRelicState();
}

void UNPRelicSlotComponent::OnRep_SpawnedRelic()
{
	ApplyRelicState();
}

void UNPRelicSlotComponent::OnRep_IsRelicReleased()
{
	ApplyRelicState();
}

void UNPRelicSlotComponent::ApplyRelicState()
{
	if (!IsValid(SpawnedRelic))
	{
		return;
	}

	const bool bCanAccessRelic = bInitiallyAccessible
		|| bIsRelicReleased
		|| bCaseAccessible;
	if (UPrimitiveComponent* RelicPrimitive =
		Cast<UPrimitiveComponent>(SpawnedRelic->GetRootComponent()))
	{
		RelicPrimitive->SetCollisionResponseToChannel(
			ECC_Destructible,
			bCanAccessRelic ? ECR_Block : ECR_Ignore);
	}
	SpawnedRelic->SetActorEnableCollision(bCanAccessRelic);
}
