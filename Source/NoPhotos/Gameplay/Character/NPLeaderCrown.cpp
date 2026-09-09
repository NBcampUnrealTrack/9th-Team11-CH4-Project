#include "Gameplay/Character/NPLeaderCrown.h"

#include "Components/StaticMeshComponent.h"
#include "Core/GameplayTag/NPGameplayTags.h"

ANPLeaderCrown::ANPLeaderCrown()
{
	GameplayCueTag = NPGameplayTags::GameplayCue_Status_Leader;
	GameplayCueName = GameplayCueTag.GetTagName();
	SceneRoot->SetAbsolute(false, true, false);
	CrownMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CrownMesh"));
	CrownMesh->SetupAttachment(SceneRoot);
	CrownMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CrownMesh->SetGenerateOverlapEvents(false);
	CrownMesh->SetCanEverAffectNavigation(false);
}

void ANPLeaderCrown::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	CrownMesh->SetRelativeLocation(FVector(0.0f, 0.0f, VisualHeight));
}

void ANPLeaderCrown::PrepareVisual()
{
	SetOwner(GetVisualTarget());
	CrownMesh->SetRelativeLocation(FVector(0.0f, 0.0f, VisualHeight));
	SetVisibleToOwner(bVisibleToOwner);
	if (!bBaseCrownScaleInitialized)
	{
		BaseCrownScale = CrownMesh->GetRelativeScale3D();
		bBaseCrownScaleInitialized = true;
	}
}

void ANPLeaderCrown::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!IsActorTickEnabled())
	{
		return;
	}
	AddActorLocalRotation(FRotator(
		0.0f,
		RotationSpeed * FMath::Max(DeltaSeconds, 0.0f),
		0.0f));
}

void ANPLeaderCrown::SetVisibleToOwner(bool bNewVisibleToOwner)
{
	bVisibleToOwner = bNewVisibleToOwner;
	CrownMesh->SetOwnerNoSee(!bVisibleToOwner);
}

void ANPLeaderCrown::ApplyVisualScale()
{
	if (!bBaseCrownScaleInitialized)
	{
		BaseCrownScale = CrownMesh->GetRelativeScale3D();
		bBaseCrownScaleInitialized = true;
	}
	CrownMesh->SetRelativeScale3D(BaseCrownScale * GetVisualScaleMultiplier());
}
