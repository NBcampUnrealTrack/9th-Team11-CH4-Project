#include "Gameplay/Relic/Gimmick/Ship/NPShipGimmickBase.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Gameplay/Interaction/Components/GrabbableComponent.h"
#include "Gameplay/Relic/Gimmick/Ship/NPShipGimmickComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"

ANPShipGimmickBase::ANPShipGimmickBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	GimmickMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GimmickMesh"));
	GimmickMesh->SetupAttachment(SceneRoot);
	GimmickMesh->SetMobility(EComponentMobility::Movable);
	GimmickMesh->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	GimmickMesh->SetEnableGravity(false);
	GimmickMesh->SetIsReplicated(true);
	GimmickMesh->BodyInstance.bAutoWeld = false;

	PhysicsConstraint = CreateDefaultSubobject<UPhysicsConstraintComponent>(
		TEXT("PhysicsConstraint"));
	PhysicsConstraint->SetupAttachment(SceneRoot);
	PhysicsConstraint->SetDisableCollision(true);

	GrabbableComponent = CreateDefaultSubobject<UGrabbableComponent>(
		TEXT("GrabbableComponent"));
	GimmickComponent = CreateDefaultSubobject<UNPShipGimmickComponent>(
		TEXT("GimmickComponent"));
}

void ANPShipGimmickBase::BeginPlay()
{
	Super::BeginPlay();

	if (IsValid(GrabbableComponent))
	{
		GrabbableComponent->OnGrabStarted.AddUObject(
			this,
			&ThisClass::HandleGrabStarted);
		GrabbableComponent->OnGrabEnded.AddUObject(
			this,
			&ThisClass::HandleGrabEnded);
	}

	if (IsValid(GimmickComponent))
	{
		GimmickComponent->OnCompleted.AddUObject(
			this,
			&ThisClass::HandleGimmickCompleted);
	}

	if (HasAuthority())
	{
		SetGimmickPhysicsEnabled(bStartWithPhysicsEnabled);
	}
	else
	{
		GimmickMesh->SetSimulatePhysics(false);
	}
}

bool ANPShipGimmickBase::IsGimmickGrabbed() const
{
	return GrabbableComponent && GrabbableComponent->IsGrabbed();
}

void ANPShipGimmickBase::CompleteShipGimmick()
{
	if (GimmickComponent)
	{
		GimmickComponent->CompleteGimmick();
	}
}

void ANPShipGimmickBase::NotifyShipGimmickActivated()
{
	OnActivated.Broadcast(this);
	ReceiveShipGimmickActivated();
}

void ANPShipGimmickBase::ResetShipGimmick()
{
	if (!HasAuthority())
	{
		return;
	}

	OnReset.Broadcast();
	ReceiveShipGimmickReset();
}

void ANPShipGimmickBase::SetGimmickPhysicsEnabled(const bool bEnabled)
{
	if (!HasAuthority() || !GimmickMesh)
	{
		return;
	}

	if (bEnabled)
	{
		ConfigurePhysicsConstraint();
		GimmickMesh->SetEnableGravity(false);
		GimmickMesh->SetSimulatePhysics(true);
		GimmickMesh->WakeAllRigidBodies();
	}
	else
	{
		if (IsValid(GrabbableComponent))
		{
			GrabbableComponent->ForceReleaseAllGrabs();
		}
		GimmickMesh->SetSimulatePhysics(false);
	}

	ForceNetUpdate();
}

void ANPShipGimmickBase::HandleGrabStarted(UPrimitiveComponent*)
{
	OnGrabStateChanged.Broadcast(true);
	ReceiveGrabStateChanged(true);
}

void ANPShipGimmickBase::HandleGrabEnded()
{
	OnGrabStateChanged.Broadcast(false);
	ReceiveGrabStateChanged(false);
}

void ANPShipGimmickBase::HandleGimmickCompleted()
{
	OnCompleted.Broadcast();
	ReceiveShipGimmickCompleted();
}

void ANPShipGimmickBase::ConfigurePhysicsConstraint()
{
	if (!PhysicsConstraint || !GimmickMesh)
	{
		return;
	}
	
	PhysicsConstraint->SetConstrainedComponents(
		GimmickMesh,
		NAME_None,
		nullptr,
		NAME_None);
}
