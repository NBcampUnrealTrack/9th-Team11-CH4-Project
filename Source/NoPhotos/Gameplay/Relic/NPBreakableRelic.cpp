#include "Gameplay/Relic/NPBreakableRelic.h"

#include "Gameplay/Interaction/Components/GrabbableComponent.h"
#include "Gameplay/Relic/Components/NPImpactReceiveComponent.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "Net/UnrealNetwork.h"
#if WITH_EDITORONLY_DATA
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#endif
#if WITH_EDITOR
#include "UObject/UnrealType.h"
#endif

ANPBreakableRelic::ANPBreakableRelic(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RelicMesh->SetGenerateOverlapEvents(true);
	RelicMesh->SetNotifyRigidBodyCollision(true);

	GeometryCollectionComponent = CreateDefaultSubobject<UGeometryCollectionComponent>(
		TEXT("BrokenGeometry"));
	GeometryCollectionComponent->SetupAttachment(RelicMesh);
	GeometryCollectionComponent->SetVisibility(false, true);
	GeometryCollectionComponent->SetHiddenInGame(true, true);
	GeometryCollectionComponent->SetGenerateOverlapEvents(false);
	GeometryCollectionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GeometryCollectionComponent->SetSimulatePhysics(false);
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

void ANPBreakableRelic::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UpdateImpactThresholdsFromMass();
}

void ANPBreakableRelic::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	ImpactReceiveComponent->SetImpactTargetComponent(RelicMesh);
	UpdateImpactThresholdsFromMass();
}

void ANPBreakableRelic::BeginPlay()
{
	Super::BeginPlay();

	if (BrokenGeometryAsset
		&& GeometryCollectionComponent->GetRestCollection()
			!= BrokenGeometryAsset.Get())
	{
		GeometryCollectionComponent->SetRestCollection(BrokenGeometryAsset);
	}

	if (GeometryCollectionComponent->GetRestCollection())
	{
		if (HasAuthority())
		{
			GeometryCollectionComponent->OnFullyDecayedEvent.AddDynamic(
				this,
				&ANPBreakableRelic::HandleFullyDecayed);
		}
		GeometryCollectionComponent->ForceBrokenForCustomRenderer(false);
		GeometryCollectionComponent->SetEnableDamageFromCollision(false);
		GeometryCollectionComponent->SetNotifyBreaks(true);
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

void ANPBreakableRelic::RefreshGeometrySource()
{
#if WITH_EDITOR
	SyncGeometrySource();
#endif
}

#if WITH_EDITOR
void ANPBreakableRelic::PostEditChangeProperty(
	FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetPropertyName()
		== GET_MEMBER_NAME_CHECKED(ANPBreakableRelic, BrokenGeometryAsset))
	{
		SyncGeometrySource();
	}
	else if (PropertyChangedEvent.GetPropertyName()
		== GET_MEMBER_NAME_CHECKED(
			ANPBreakableRelic,
			MinImpactMassMultiplier)
		|| PropertyChangedEvent.GetPropertyName()
			== GET_MEMBER_NAME_CHECKED(
				ANPBreakableRelic,
				MaxImpactMassMultiplier))
	{
		UpdateImpactThresholdsFromMass();
	}
}
#endif

void ANPBreakableRelic::OnRep_IsBroken()
{
	// Multicast를 놓치는 늦은 접속 클라이언트를 위한 보조 경로입니다.
	ApplyBrokenState();
}

void ANPBreakableRelic::HandleFullyDecayed()
{
	if (HasAuthority())
	{
		SetLifeSpan(0.01f);
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
	const float RemainingHealthRatio = MaxHealth > 0
		? FMath::Clamp(
			static_cast<float>(CurrentHealth) / static_cast<float>(MaxHealth),
			0.0f,
			1.0f)
		: 0.0f;
	OnRelicDamaged(
		Damage,
		CurrentHealth,
		MaxHealth,
		RemainingHealthRatio);
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
	const FTransform IntactTransform = RelicMesh->GetComponentTransform();
	const FVector LinearVelocity = RelicMesh->GetPhysicsLinearVelocity();
	const FVector AngularVelocity =
		RelicMesh->GetPhysicsAngularVelocityInRadians();

	RelicMesh->SetSimulatePhysics(false);
	RelicMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RelicMesh->SetVisibility(false, true);

	GeometryCollectionComponent->DetachFromComponent(
		FDetachmentTransformRules::KeepWorldTransform);
	GeometryCollectionComponent->SetWorldTransform(IntactTransform);
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
	GeometryCollectionComponent->SetPhysicsLinearVelocity(LinearVelocity);
	GeometryCollectionComponent->SetPhysicsAngularVelocityInRadians(
		AngularVelocity);
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

void ANPBreakableRelic::SyncGeometrySource()
{
#if WITH_EDITORONLY_DATA
	UStaticMeshComponent* StaticRelicMesh =
		Cast<UStaticMeshComponent>(RelicMesh);
	if (!StaticRelicMesh || !GeometryCollectionComponent)
	{
		return;
	}

	GeometryCollectionComponent->SetRestCollection(BrokenGeometryAsset);
	StaticRelicMesh->EmptyOverrideMaterials();

	if (!BrokenGeometryAsset)
	{
		StaticRelicMesh->SetStaticMesh(nullptr);
		return;
	}

	if (BrokenGeometryAsset->GeometrySource.Num() != 1)
	{
		return;
	}

	const FGeometryCollectionSource& GeometrySource =
		BrokenGeometryAsset->GeometrySource[0];
	UStaticMesh* SourceStaticMesh = Cast<UStaticMesh>(
		GeometrySource.SourceGeometryObject.TryLoad());
	if (!SourceStaticMesh)
	{
		return;
	}

	StaticRelicMesh->SetStaticMesh(SourceStaticMesh);
	for (int32 MaterialIndex = 0;
		MaterialIndex < GeometrySource.SourceMaterial.Num();
		++MaterialIndex)
	{
		StaticRelicMesh->SetMaterial(
			MaterialIndex,
			GeometrySource.SourceMaterial[MaterialIndex]);
	}

	UpdateImpactThresholdsFromMass();
#endif
}

void ANPBreakableRelic::UpdateImpactThresholdsFromMass()
{
	if (!RelicMesh || !ImpactReceiveComponent)
	{
		return;
	}

	const float MeshMass = RelicMesh->CalculateMass();
	if (MeshMass <= UE_KINDA_SMALL_NUMBER)
	{
		return;
	}

	ImpactReceiveComponent->SetImpactThresholds(
		MeshMass * MinImpactMassMultiplier,
		MeshMass * MaxImpactMassMultiplier);
}
