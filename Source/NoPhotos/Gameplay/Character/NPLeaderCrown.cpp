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
