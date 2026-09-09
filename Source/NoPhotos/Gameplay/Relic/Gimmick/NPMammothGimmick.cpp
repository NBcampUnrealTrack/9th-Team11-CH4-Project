#include "Gameplay/Relic/Gimmick/NPMammothGimmick.h"

#include "Components/SceneComponent.h"
#include "Gameplay/Interaction/Components/GrabbableComponent.h"
#include "Gameplay/Relic/Gimmick/Components/NPPullGimmickComponent.h"
#include "Gameplay/Relic/Components/NPRelicSlotComponent.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "Net/UnrealNetwork.h"

ANPMammothGimmick::ANPMammothGimmick()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	BoneGeometry = CreateDefaultSubobject<UGeometryCollectionComponent>(TEXT("BoneGeometry"));
	BoneGeometry->SetupAttachment(SceneRoot);
	BoneGeometry->SetEnableReplication(true);
	BoneGeometry->ObjectType = EObjectStateTypeEnum::Chaos_Object_Kinematic;
	BoneGeometry->SetEnableDamageFromCollision(false);
	BoneGeometry->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	BoneGeometry->SetSimulatePhysics(true);
}

void ANPMammothGimmick::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANPMammothGimmick, bCollapsed);
	DOREPLIFETIME(ANPMammothGimmick, DisabledPullActors);
}

void ANPMammothGimmick::BeginPlay()
{
	Super::BeginPlay();

	BoneGeometry->SetMobility(EComponentMobility::Movable);
	BoneGeometry->SetEnableReplication(true);
	BoneGeometry->SetIsReplicated(true);
	BoneGeometry->SetReplicationAbandonAfterLevel(100);
	BoneGeometry->SetReplicationMaxPositionAndVelocityCorrectionLevel(0);
	BoneGeometry->ObjectType = EObjectStateTypeEnum::Chaos_Object_Kinematic;
	BoneGeometry->SetEnableDamageFromCollision(false);
	BoneGeometry->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	BoneGeometry->SetEnableGravity(true);
	BoneGeometry->SetSimulatePhysics(false);
	BoneGeometry->RecreatePhysicsState();
	BoneGeometry->SetSimulatePhysics(true);
	bGeometryInitialized = true;

	if (bCollapsed)
	{
		OnRep_Collapsed();
	}
	else
	{
		BoneGeometry->SetDynamicState(Chaos::EObjectStateType::Kinematic);
		const int32 RootIndex = BoneGeometry->GetRootIndex();
		if (RootIndex != INDEX_NONE)
		{
			BoneGeometry->SetAnchoredByIndex(RootIndex, true);
		}
	}

	if (!HasAuthority())
	{
		return;
	}

	for (AActor* GimmickActor : PullGimmickActors)
	{
		if (IsValid(GimmickActor))
		{
			if (UNPPullGimmickComponent* Pull = GimmickActor->FindComponentByClass<UNPPullGimmickComponent>())
			{
				Pull->OnCompleted.AddUObject(this, &ANPMammothGimmick::HandlePullCompleted);
			}
		}
	}
	HandlePullCompleted();
}

void ANPMammothGimmick::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (AActor* GimmickActor : PullGimmickActors)
	{
		if (IsValid(GimmickActor))
		{
			if (UNPPullGimmickComponent* Pull = GimmickActor->FindComponentByClass<UNPPullGimmickComponent>())
			{
				Pull->OnCompleted.RemoveAll(this);
			}
		}
	}
	Super::EndPlay(EndPlayReason);
}

void ANPMammothGimmick::HandlePullCompleted()
{
	if (!HasAuthority() || bCollapsed || PullGimmickActors.IsEmpty())
	{
		return;
	}
	bool bAllCompleted = true;
	for (AActor* GimmickActor : PullGimmickActors)
	{
		const UNPPullGimmickComponent* Pull = IsValid(GimmickActor)
			? GimmickActor->FindComponentByClass<UNPPullGimmickComponent>() : nullptr;
		if (!Pull || !Pull->IsCompleted())
		{
			bAllCompleted = false;
			continue;
		}
		DisablePullActor(GimmickActor);
	}
	if (bAllCompleted)
	{
		Collapse();
	}
}

void ANPMammothGimmick::DisablePullActor(AActor* GimmickActor)
{
	if (!IsValid(GimmickActor) || DisabledPullActors.Contains(GimmickActor))
	{
		return;
	}
	DisabledPullActors.Add(GimmickActor);
	OnRep_DisabledPullActors();
	ForceNetUpdate();
}

void ANPMammothGimmick::OnRep_DisabledPullActors()
{
	for (AActor* GimmickActor : DisabledPullActors)
	{
		if (!IsValid(GimmickActor))
		{
			continue;
		}
		if (UGrabbableComponent* Grabbable = GimmickActor->FindComponentByClass<UGrabbableComponent>())
		{
			Grabbable->SetGrabEnabled(false);
		}
		GimmickActor->SetActorEnableCollision(false);
	}
}

void ANPMammothGimmick::Collapse()
{
	if (!HasAuthority() || bCollapsed)
	{
		return;
	}
	bCollapsed = true;
	for (AActor* GimmickActor : PullGimmickActors)
	{
		DisablePullActor(GimmickActor);
	}
	MulticastCollapse();
	ForceNetUpdate();

	TArray<UNPRelicSlotComponent*> RelicSlots;
	GetComponents<UNPRelicSlotComponent>(RelicSlots);
	for (UNPRelicSlotComponent* RelicSlot : RelicSlots)
	{
		if (IsValid(RelicSlot))
		{
			RelicSlot->SpawnRelic(true);
		}
	}
}

void ANPMammothGimmick::MulticastCollapse_Implementation()
{
	bCollapsed = true;
	OnRep_Collapsed();
}

void ANPMammothGimmick::OnRep_Collapsed()
{
	if (!bGeometryInitialized || !bCollapsed || bCollapseApplied)
	{
		return;
	}
	bCollapseApplied = true;
	OnRep_DisabledPullActors();
	BoneGeometry->SetMobility(EComponentMobility::Movable);
	BoneGeometry->SetVisibility(true, true);
	BoneGeometry->SetHiddenInGame(false, true);
	BoneGeometry->Activate(true);
	BoneGeometry->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	BoneGeometry->SetCollisionObjectType(ECC_WorldDynamic);
	BoneGeometry->ObjectType = EObjectStateTypeEnum::Chaos_Object_Dynamic;
	BoneGeometry->SetEnableGravity(true);
	BoneGeometry->SetSimulatePhysics(true);
	BoneGeometry->RemoveAllAnchors();
	BoneGeometry->SetDynamicState(Chaos::EObjectStateType::Dynamic);
	BoneGeometry->WakeAllRigidBodies();
	BoneGeometry->ForceBrokenForCustomRenderer(true);
	const int32 RootIndex = BoneGeometry->GetRootIndex();
	if (RootIndex != INDEX_NONE)
	{
		BoneGeometry->CrumbleCluster(RootIndex);
	}
	OnCollapsed();
}
