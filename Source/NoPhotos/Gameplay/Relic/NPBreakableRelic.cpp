#include "Gameplay/Relic/NPBreakableRelic.h"

#include "Gameplay/Interaction/Components/GrabbableComponent.h"
#include "Gameplay/Relic/Components/NPImpactReceiveComponent.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "Net/UnrealNetwork.h"

ANPBreakableRelic::ANPBreakableRelic(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<
		UGeometryCollectionComponent>(RelicComponentName))
{
	GeometryCollectionComponent = CastChecked<UGeometryCollectionComponent>(RelicMesh);
	// 루트 위치만 보정하고 파편은 각 환경에서 별도로 시뮬레이션합니다.
	GeometryCollectionComponent->SetEnableReplication(true);
	GeometryCollectionComponent->SetReplicationAbandonAfterLevel(100);
	GeometryCollectionComponent->SetReplicationMaxPositionAndVelocityCorrectionLevel(0);
	GeometryCollectionComponent->ObjectType =
		EObjectStateTypeEnum::Chaos_Object_Dynamic;
	GeometryCollectionComponent->SetCollisionResponseToChannel(
		ECC_Destructible,
		ECR_Ignore);
	GeometryCollectionComponent->SetNotifyRigidBodyCollision(true);

	ImpactReceiveComponent = CreateDefaultSubobject<UNPImpactReceiveComponent>(
		TEXT("ImpactReceiveComponent"));
}

void ANPBreakableRelic::BeginPlay()
{
	Super::BeginPlay();

	if (GeometryCollectionComponent->GetRestCollection())
	{
		if (HasAuthority())
		{
			GeometryCollectionComponent->OnFullyDecayedEvent.AddDynamic(
				this,
				&ANPBreakableRelic::HandleFullyDecayed);
		}
		GeometryCollectionComponent->SetVisibility(true, true);
		GeometryCollectionComponent->SetHiddenInGame(false, true);
		GeometryCollectionComponent->ForceBrokenForCustomRenderer(false);
		GeometryCollectionComponent->SetEnableDamageFromCollision(false);
		GeometryCollectionComponent->SetNotifyBreaks(true);
		GeometryCollectionComponent->SetCollisionEnabled(
			ECollisionEnabled::QueryAndPhysics);
		GeometryCollectionComponent->SetEnableGravity(true);

		if (!bIsBroken)
		{
			GeometryCollectionComponent->SetSimulatePhysics(true);
			if (IsDisplayed())
			{
				const int32 RootIndex = GeometryCollectionComponent->GetRootIndex();
				if (RootIndex != INDEX_NONE)
				{
					GeometryCollectionComponent->SetAnchoredByIndex(
						RootIndex,
						true);
				}
			}
		}
	}

	ApplyBrokenState();
	ImpactReceiveComponent->OnDamaged.AddUObject(
		this,
		&ANPBreakableRelic::HandleDurabilityDamaged);
	ImpactReceiveComponent->OnDepleted.AddUObject(
		this,
		&ANPBreakableRelic::HandleDurabilityDepleted);
	GrabbableComponent->OnGrabStarted.AddUObject(
		this,
		&ANPBreakableRelic::HandleBreakableGrabStarted);
}

void ANPBreakableRelic::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANPBreakableRelic, bIsBroken);
	DOREPLIFETIME(ANPBreakableRelic, BreakLocation);
}

FVector ANPBreakableRelic::GetRelicWorldLocation() const
{
	if (!IsValid(GeometryCollectionComponent)
		|| !GeometryCollectionComponent->GetRestCollection())
	{
		return Super::GetRelicWorldLocation();
	}

	return bIsBroken
		? GeometryCollectionComponent->Bounds.Origin
		: GeometryCollectionComponent->GetRootCurrentTransform().GetLocation();
}

void ANPBreakableRelic::OnRep_IsBroken()
{
	// Multicast를 놓치는 늦은 접속 클라이언트를 위한 보조 경로입니다.
	ApplyBrokenState();
}

void ANPBreakableRelic::HandleFullyDecayed()
{
	if (HasAuthority())
	{
		Destroy();
	}
}

void ANPBreakableRelic::MulticastBreakRelic_Implementation(
	const FVector_NetQuantize10 InBreakLocation)
{
	BreakLocation = InBreakLocation;
	bIsBroken = true;
	ApplyBrokenState();
}

void ANPBreakableRelic::HandleDurabilityDamaged(
	const int32 Damage,
	const int32 CurrentHealth,
	const int32 MaxHealth)
{
	OnRelicDamaged(Damage, CurrentHealth, MaxHealth);
}

void ANPBreakableRelic::HandleDurabilityDepleted(
	const FVector& ImpactLocation)
{
	BreakRelic(ImpactLocation);
}

void ANPBreakableRelic::HandleBreakableGrabStarted(
	UPrimitiveComponent* GrabbedComponent)
{
	ImpactReceiveComponent->IgnoreGrabImpact();
	if (GeometryCollectionComponent)
	{
		GeometryCollectionComponent->RemoveAllAnchors();
	}
}

void ANPBreakableRelic::BreakRelic(
	const FVector& ImpactLocation)
{
	if (!HasAuthority() || bIsBroken)
	{
		return;
	}

	BreakLocation = ImpactLocation;
	bIsBroken = true;
	MulticastBreakRelic(BreakLocation);
	ForceNetUpdate();
}

void ANPBreakableRelic::ApplyBrokenState()
{
	if (!bIsBroken)
	{
		return;
	}
	GrabbableComponent->SetGrabEnabled(false);

	if (!bBrokenEventDispatched)
	{
		bBrokenEventDispatched = true;
		OnRelicBroken();
	}

	if (bClusterBreakApplied)
	{
		return;
	}

	if (!IsValid(GeometryCollectionComponent))
	{
		return;
	}

	bClusterBreakApplied = true;
	GeometryCollectionComponent->SetMobility(EComponentMobility::Movable);
	GeometryCollectionComponent->SetVisibility(true, true);
	GeometryCollectionComponent->SetHiddenInGame(false, true);
	GeometryCollectionComponent->Activate(true);

	GeometryCollectionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GeometryCollectionComponent->SetCollisionObjectType(ECC_Destructible);
	GeometryCollectionComponent->ObjectType = EObjectStateTypeEnum::Chaos_Object_Dynamic;
	GeometryCollectionComponent->SetEnableGravity(true);
	// Cluster를 해제하기 전에 동적 물리 상태를 보장합니다.
	GeometryCollectionComponent->SetSimulatePhysics(true);
	GeometryCollectionComponent->RemoveAllAnchors();
	GeometryCollectionComponent->WakeAllRigidBodies();
	GeometryCollectionComponent->ForceBrokenForCustomRenderer(true);

	BreakRootCluster();
}

void ANPBreakableRelic::BreakRootCluster()
{
	if (!IsValid(GeometryCollectionComponent) || !bIsBroken)
	{
		return;
	}

	GeometryCollectionComponent->SetEnableGravity(true);
	GeometryCollectionComponent->SetSimulatePhysics(true);
	GeometryCollectionComponent->WakeAllRigidBodies();
	const int32 RootIndex = GeometryCollectionComponent->GetRootIndex();
	if (RootIndex == INDEX_NONE)
	{
		return;
	}

	GeometryCollectionComponent->CrumbleCluster(RootIndex);
}
