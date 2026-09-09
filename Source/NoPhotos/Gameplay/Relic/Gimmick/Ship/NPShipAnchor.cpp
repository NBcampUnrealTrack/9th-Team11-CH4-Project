#include "Gameplay/Relic/Gimmick/Ship/NPShipAnchor.h"

#include "CableComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Gameplay/Interaction/Components/GrabbableComponent.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "UObject/ConstructorHelpers.h"

ANPShipAnchor::ANPShipAnchor()
{
	GimmickMesh->SetupAttachment(nullptr);
	SetRootComponent(GimmickMesh);
	SceneRoot->SetupAttachment(GimmickMesh);
	bStartWithPhysicsEnabled = false;
	GimmickMesh->SetEnableGravity(true);
	GimmickMesh->SetGenerateOverlapEvents(true);
	GimmickMesh->SetRelativeScale3D(FVector(2.0));
	GimmickMesh->BodyInstance.bOverrideMass = true;
	GimmickMesh->BodyInstance.SetMassOverride(50.0f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshAsset(TEXT(
		"/Game/NoPhotos/Resources/Exturnal/PolyUniversalPack/Meshes/Fantasy/"
		"Castle_Fantasy/Scaffolding_Fantasy/SM_Anchor_Plate_Cross_Fantasy"));
	if (MeshAsset.Succeeded())
	{
		GimmickMesh->SetStaticMesh(MeshAsset.Object);
	}

	AnchorRope = CreateDefaultSubobject<UCableComponent>(TEXT("AnchorRope"));
	AnchorRope->SetupAttachment(GimmickMesh);
	AnchorRope->CableLength = 300.0f;
	AnchorRope->CableWidth = 10.0f;
	AnchorRope->NumSegments = 10;
	AnchorRope->EndLocation = FVector(15.0, 0.0, 30.0);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> RopeMaterial(TEXT(
		"/Game/MafiaBarnPack/Materials/Architecture/MI_Rope"));
	if (RopeMaterial.Succeeded())
	{
		AnchorRope->SetMaterial(0, RopeMaterial.Object);
	}
}

void ANPShipAnchor::BeginPlay()
{
	InitialMeshTransform = GimmickMesh->GetComponentTransform();
	Super::BeginPlay();

	PhysicsConstraint->BreakConstraint();
	ConfigureRope();
	OnGrabStateChanged.AddDynamic(this, &ThisClass::HandleAnchorGrabChanged);

	if (HasAuthority())
	{
		AnchorState.Transform = InitialMeshTransform;
		ApplyAnchorState();
		SetTargetVisible(false);
		if (IsValid(AnchorZone))
		{
			AnchorZone->OnActorBeginOverlap.AddDynamic(this, &ThisClass::HandleZoneOverlap);
			ensureMsgf(FindZoneComponent(SnapPointComponentName),
				TEXT("%s: AnchorZone must contain a scene component named %s"),
				*GetName(), *SnapPointComponentName.ToString());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: assign AnchorZone to enable anchor placement."), *GetName());
		}
	}
	else
	{
		OnRep_AnchorState();
		OnRep_ShowTarget();
	}
}

void ANPShipAnchor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(AnchorZone))
	{
		AnchorZone->OnActorBeginOverlap.RemoveDynamic(this, &ThisClass::HandleZoneOverlap);
	}
	OnGrabStateChanged.RemoveDynamic(this, &ThisClass::HandleAnchorGrabChanged);
	Super::EndPlay(EndPlayReason);
}

void ANPShipAnchor::ConfigureRope()
{
	if (!IsValid(RopeStart) || !RopeStart->GetRootComponent())
	{
		AnchorRope->SetVisibility(false);
		return;
	}
	AnchorRope->AttachToComponent(RopeStart->GetRootComponent(),
		FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	AnchorRope->SetRelativeScale3D(FVector::OneVector);
	AnchorRope->SetAttachEndToComponent(GimmickMesh);
	AnchorRope->SetVisibility(true);
}

USceneComponent* ANPShipAnchor::FindZoneComponent(const FName ComponentName) const
{
	if (IsValid(AnchorZone))
	{
		TInlineComponentArray<USceneComponent*> Components(AnchorZone);
		for (USceneComponent* Component : Components)
		{
			if (Component->GetFName() == ComponentName)
			{
				return Component;
			}
		}
	}
	return nullptr;
}

void ANPShipAnchor::HandleZoneOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	if (!HasAuthority() || OverlappedActor != AnchorZone || OtherActor != this)
	{
		return;
	}
	if (const USceneComponent* SnapPoint = FindZoneComponent(SnapPointComponentName))
	{
		FTransform SnapTransform = SnapPoint->GetComponentTransform();
		SnapTransform.SetScale3D(InitialMeshTransform.GetScale3D());
		PlaceAnchor(SnapTransform);
	}
}

void ANPShipAnchor::PlaceAnchor(const FTransform& SnapTransform)
{
	if (!HasAuthority() || AnchorState.bPlaced || bChangingState)
	{
		return;
	}
	{
		TGuardValue<bool> StateGuard(bChangingState, true);
		AnchorState.bPlaced = true;
		AnchorState.Transform = SnapTransform;
		++AnchorState.Revision;
		GrabbableComponent->SetGrabEnabled(false); // Releases every player before snapping.
		ApplyAnchorState();
		SetTargetVisible(false);
		ForceNetUpdate();
	}

	NotifyShipGimmickActivated();
}

void ANPShipAnchor::ApplyAnchorState()
{
	PhysicsConstraint->BreakConstraint();
	if (GimmickMesh->IsSimulatingPhysics())
	{
		GimmickMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
		GimmickMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}
	GimmickMesh->SetSimulatePhysics(false);
	GimmickMesh->SetWorldTransform(AnchorState.Transform, false, nullptr, ETeleportType::TeleportPhysics);
	GimmickMesh->SetEnableGravity(true);
	GimmickMesh->SetSimulatePhysics(HasAuthority() && !AnchorState.bPlaced);
	if (GimmickMesh->IsSimulatingPhysics())
	{
		GimmickMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
		GimmickMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		GimmickMesh->WakeAllRigidBodies();
	}
	GrabbableComponent->SetGrabEnabled(!AnchorState.bPlaced);
}

void ANPShipAnchor::HandleAnchorGrabChanged(const bool bIsGrabbed)
{
	if (!HasAuthority() || bChangingState)
	{
		return;
	}
	SetTargetVisible(bIsGrabbed && !AnchorState.bPlaced);
}

void ANPShipAnchor::SetTargetVisible(const bool bVisible)
{
	bShowTarget = bVisible;
	OnRep_ShowTarget();
	ForceNetUpdate();
}

void ANPShipAnchor::OnRep_ShowTarget()
{
	if (USceneComponent* Visual = FindZoneComponent(TargetVisualComponentName))
	{
		Visual->SetVisibility(bShowTarget);
	}
}

void ANPShipAnchor::OnRep_AnchorState()
{
	TGuardValue<bool> StateGuard(bChangingState, true);
	GrabbableComponent->ForceReleaseAllGrabs();
	GrabbableComponent->SetGrabEnabled(!AnchorState.bPlaced);
	OnRep_ReplicatedMovement();
}

void ANPShipAnchor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANPShipAnchor, AnchorState);
	DOREPLIFETIME(ANPShipAnchor, bShowTarget);
}
