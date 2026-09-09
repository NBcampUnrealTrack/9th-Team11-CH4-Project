#include "Gameplay/Character/NPLeaderCrown.h"

#include "Components/StaticMeshComponent.h"

ANPLeaderCrown::ANPLeaderCrown()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	CrownMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CrownMesh"));
	SetRootComponent(CrownMesh);
	CrownMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CrownMesh->SetGenerateOverlapEvents(false);
	CrownMesh->SetCanEverAffectNavigation(false);
}

void ANPLeaderCrown::BeginPlay()
{
	Super::BeginPlay();
	SetOwner(GetParentActor());
	SetVisibleToOwner(bVisibleToOwner);
	if (!bBaseCrownScaleInitialized)
	{
		BaseCrownScale = CrownMesh->GetRelativeScale3D();
		bBaseCrownScaleInitialized = true;
	}
}

void ANPLeaderCrown::SetVisibleToOwner(bool bNewVisibleToOwner)
{
	bVisibleToOwner = bNewVisibleToOwner;
	CrownMesh->SetOwnerNoSee(!bVisibleToOwner);
}

void ANPLeaderCrown::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!IsHidden())
	{
		AddActorLocalRotation(FRotator(0.0f, RotationSpeed * DeltaSeconds, 0.0f));
	}
}

void ANPLeaderCrown::SetVisualScaleMultiplier(float ScaleMultiplier)
{
	if (!bBaseCrownScaleInitialized)
	{
		BaseCrownScale = CrownMesh->GetRelativeScale3D();
		bBaseCrownScaleInitialized = true;
	}
	CrownMesh->SetRelativeScale3D(BaseCrownScale * ScaleMultiplier);
}
