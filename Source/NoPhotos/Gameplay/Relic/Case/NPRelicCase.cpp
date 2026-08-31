#include "Gameplay/Relic/Case/NPRelicCase.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Gameplay/Relic/Components/NPImpactReceiveComponent.h"
#include "Gameplay/Relic/Components/NPRelicSlotComponent.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "Net/UnrealNetwork.h"

ANPRelicCase::ANPRelicCase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	GeometryScene = CreateDefaultSubobject<USceneComponent>(
		TEXT("GeometryScene"));
	GeometryScene->SetupAttachment(SceneRoot);

	ImpactReceiveComponent = CreateDefaultSubobject<UNPImpactReceiveComponent>(
		TEXT("ImpactReceiveComponent"));

	RelicScene = CreateDefaultSubobject<USceneComponent>(TEXT("RelicScene"));
	RelicScene->SetupAttachment(SceneRoot);
}

void ANPRelicCase::CreateConfiguredRelics()
{
#if WITH_EDITOR
	UWorld* World = GetWorld();
	if (!World || World->IsGameWorld())
	{
		return;
	}

	CollectRelicSlots();
	for (UNPRelicSlotComponent* RelicSlot : RelicSlots)
	{
		if (IsValid(RelicSlot))
		{
			RelicSlot->RecreateRelicInEditor();
		}
	}
#endif
}

void ANPRelicCase::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	CollectCaseGeometryCollections();
	CollectRelicSlots();

	TArray<UPrimitiveComponent*> ImpactTargets;
	ImpactTargets.Reserve(CaseGeometryCollections.Num());
	for (UGeometryCollectionComponent* GeometryCollection : CaseGeometryCollections)
	{
		ImpactTargets.Add(GeometryCollection);
	}
	ImpactReceiveComponent->SetImpactTargetComponents(ImpactTargets);
}

void ANPRelicCase::BeginPlay()
{
	Super::BeginPlay();

	ImpactReceiveComponent->OnDamaged.AddUObject(
		this,
		&ANPRelicCase::HandleDurabilityDamaged);
	ImpactReceiveComponent->OnDepleted.AddUObject(
		this,
		&ANPRelicCase::HandleDurabilityDepleted);

	InitializeGeometryCollections();
	ApplyCaseState();

	if (HasAuthority())
	{
		SpawnContainedRelics();
	}
}

void ANPRelicCase::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ANPRelicCase, bIsBroken);
	DOREPLIFETIME(ANPRelicCase, bIsUnlocked);
	DOREPLIFETIME(ANPRelicCase, BreakLocation);
}

bool ANPRelicCase::UnlockCase()
{
	if (!HasAuthority() || bIsUnlocked || bIsBroken)
	{
		return false;
	}

	bIsUnlocked = true;
	ApplyCaseState();
	ReleaseContainedRelics();
	ForceNetUpdate();
	return true;
}

bool ANPRelicCase::LockCase()
{
	if (!HasAuthority() || !bIsUnlocked || bIsBroken)
	{
		return false;
	}

	bIsUnlocked = false;
	ApplyCaseState();
	ForceNetUpdate();
	return true;
}

bool ANPRelicCase::TrySetLocked_Implementation(const bool bLocked)
{
	return bLocked ? LockCase() : UnlockCase();
}

bool ANPRelicCase::IsLocked_Implementation() const
{
	return !IsAccessible();
}

void ANPRelicCase::OnRep_IsBroken()
{
	ApplyCaseState();
}

void ANPRelicCase::OnRep_IsUnlocked()
{
	ApplyCaseState();
}

void ANPRelicCase::MulticastBreakCase_Implementation(
	const FVector_NetQuantize10 InBreakLocation)
{
	BreakLocation = InBreakLocation;
	bIsBroken = true;
	ApplyCaseState();
}

void ANPRelicCase::CollectCaseGeometryCollections()
{
	CaseGeometryCollections.Reset();

	TArray<UGeometryCollectionComponent*> GeometryCollections;
	GetComponents(GeometryCollections);
	for (UGeometryCollectionComponent* GeometryCollection : GeometryCollections)
	{
		if (IsValid(GeometryCollection) &&
			GeometryCollection->IsAttachedTo(GeometryScene))
		{
			CaseGeometryCollections.Add(GeometryCollection);
		}
	}
}

void ANPRelicCase::InitializeGeometryCollections()
{
	for (UGeometryCollectionComponent* GeometryCollection : CaseGeometryCollections)
	{
		if (!IsValid(GeometryCollection) || !GeometryCollection->GetRestCollection())
		{
			continue;
		}

		GeometryCollection->SetMobility(EComponentMobility::Movable);
		GeometryCollection->SetEnableReplication(true);
		GeometryCollection->SetReplicationAbandonAfterLevel(100);
		GeometryCollection->SetReplicationMaxPositionAndVelocityCorrectionLevel(0);
		GeometryCollection->ObjectType =
			EObjectStateTypeEnum::Chaos_Object_Kinematic;
		GeometryCollection->SetDynamicState(
			Chaos::EObjectStateType::Kinematic);
		GeometryCollection->SetCollisionObjectType(ECC_Destructible);
		GeometryCollection->SetCollisionResponseToAllChannels(ECR_Block);
		GeometryCollection->SetCollisionResponseToChannel(
			ECC_Destructible,
			ECR_Ignore);
		GeometryCollection->SetNotifyRigidBodyCollision(true);
		GeometryCollection->SetGenerateOverlapEvents(false);
		GeometryCollection->SetVisibility(true, true);
		GeometryCollection->SetHiddenInGame(false, true);
		GeometryCollection->ForceBrokenForCustomRenderer(false);
		GeometryCollection->SetEnableDamageFromCollision(false);
		GeometryCollection->SetNotifyBreaks(true);
		GeometryCollection->SetCollisionEnabled(
			ECollisionEnabled::QueryAndPhysics);
		GeometryCollection->SetEnableGravity(true);
		GeometryCollection->SetSimulatePhysics(false);
		GeometryCollection->RecreatePhysicsState();
		GeometryCollection->SetSimulatePhysics(true);

		if (!bIsBroken)
		{
			const int32 RootIndex = GeometryCollection->GetRootIndex();
			if (RootIndex != INDEX_NONE)
			{
				GeometryCollection->SetAnchoredByIndex(RootIndex, true);
			}
		}
	}
}

void ANPRelicCase::CollectRelicSlots()
{
	RelicSlots.Reset();

	TArray<UNPRelicSlotComponent*> FoundRelicSlots;
	GetComponents(FoundRelicSlots);
	for (UNPRelicSlotComponent* RelicSlot : FoundRelicSlots)
	{
		if (IsValid(RelicSlot) && RelicSlot->IsAttachedTo(RelicScene))
		{
			RelicSlots.Add(RelicSlot);
		}
	}
}

void ANPRelicCase::HandleDurabilityDamaged(
	const int32,
	const int32 CurrentHealth,
	const int32 MaxHealth)
{
	const float RemainingHealthRatio = MaxHealth > 0
		? FMath::Clamp(
			static_cast<float>(CurrentHealth) / static_cast<float>(MaxHealth),
			0.0f,
			1.0f)
		: 0.0f;
	OnCaseDamaged(RemainingHealthRatio);
}

void ANPRelicCase::HandleDurabilityDepleted(
	const FVector& ImpactLocation)
{
	BreakCase(ImpactLocation);
}

void ANPRelicCase::BreakCase(const FVector& ImpactLocation)
{
	if (!HasAuthority() || bIsBroken)
	{
		return;
	}

	BreakLocation = ImpactLocation;
	bIsBroken = true;
	MulticastBreakCase(BreakLocation);
	ReleaseContainedRelics();
	ForceNetUpdate();
}

void ANPRelicCase::ApplyCaseState()
{
	if (bIsBroken)
	{
		ApplyBrokenState();
	}
	else if (bIsUnlocked)
	{
		bLockedEventDispatched = false;
		if (!bUnlockedEventDispatched)
		{
			bUnlockedEventDispatched = true;
			OnCaseUnlocked();
		}
	}
	else
	{
		bUnlockedEventDispatched = false;
		if (!bLockedEventDispatched)
		{
			bLockedEventDispatched = true;
			OnCaseLocked();
		}
	}
}

void ANPRelicCase::ApplyBrokenState()
{
	if (!bBrokenEventDispatched)
	{
		bBrokenEventDispatched = true;
		OnCaseBroken();
	}

	if (bBrokenStateApplied)
	{
		return;
	}
	bBrokenStateApplied = true;

	for (UGeometryCollectionComponent* GeometryCollection : CaseGeometryCollections)
	{
		if (!IsValid(GeometryCollection) || !GeometryCollection->GetRestCollection())
		{
			continue;
		}

		GeometryCollection->SetMobility(EComponentMobility::Movable);
		GeometryCollection->SetVisibility(true, true);
		GeometryCollection->SetHiddenInGame(false, true);
		GeometryCollection->Activate(true);
		GeometryCollection->SetCollisionEnabled(
			ECollisionEnabled::QueryAndPhysics);
		GeometryCollection->SetCollisionObjectType(ECC_WorldDynamic);
		GeometryCollection->ObjectType =
			EObjectStateTypeEnum::Chaos_Object_Dynamic;
		GeometryCollection->SetEnableGravity(true);
		GeometryCollection->SetSimulatePhysics(true);
		GeometryCollection->RemoveAllAnchors();
		GeometryCollection->WakeAllRigidBodies();
		GeometryCollection->ForceBrokenForCustomRenderer(true);

		const int32 RootIndex = GeometryCollection->GetRootIndex();
		if (RootIndex != INDEX_NONE)
		{
			GeometryCollection->CrumbleCluster(RootIndex);
		}
	}
}

void ANPRelicCase::SpawnContainedRelics()
{
	if (!HasAuthority())
	{
		return;
	}

	for (UNPRelicSlotComponent* RelicSlot : RelicSlots)
	{
		if (IsValid(RelicSlot))
		{
			RelicSlot->SpawnRelic(IsAccessible());
		}
	}
}

void ANPRelicCase::ReleaseContainedRelics()
{
	if (!HasAuthority())
	{
		return;
	}

	for (UNPRelicSlotComponent* RelicSlot : RelicSlots)
	{
		if (IsValid(RelicSlot))
		{
			RelicSlot->ReleaseRelic();
		}
	}
}
