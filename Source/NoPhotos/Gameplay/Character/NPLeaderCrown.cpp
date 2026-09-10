#include "Gameplay/Character/NPLeaderCrown.h"

#include "Components/StaticMeshComponent.h"
#include "Core/GameplayTag/NPGameplayTags.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"

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

	AppearEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("AppearEffect"));
	AppearEffect->SetupAttachment(SceneRoot);
	AppearEffect->SetAutoActivate(false);

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> AppearEffectAsset(
		TEXT("/Game/UnityParticle/Burst_EnergyDrain.Burst_EnergyDrain"));
	if (AppearEffectAsset.Succeeded())
	{
		AppearEffect->SetAsset(AppearEffectAsset.Object);
	}
}

void ANPLeaderCrown::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	CrownMesh->SetRelativeLocation(FVector(0.0f, 0.0f, VisualHeight));
	AppearEffect->SetRelativeLocation(FVector(0.0f, 0.0f, VisualHeight + AppearEffectZOffset));
}

void ANPLeaderCrown::PrepareVisual()
{
	SetOwner(GetVisualTarget());
	CrownMesh->SetRelativeLocation(FVector(0.0f, 0.0f, VisualHeight));
	AppearEffect->SetRelativeLocation(FVector(0.0f, 0.0f, VisualHeight + AppearEffectZOffset));
	SetVisibleToOwner(bVisibleToOwner);
	if (!bBaseCrownScaleInitialized)
	{
		BaseCrownScale = CrownMesh->GetRelativeScale3D();
		bBaseCrownScaleInitialized = true;
	}
}

void ANPLeaderCrown::OnAppearTransitionStarted()
{
	AppearEffect->Activate(true);
}

void ANPLeaderCrown::ResetVisual()
{
	AppearEffect->DeactivateImmediate();
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
	AppearEffect->SetOwnerNoSee(!bVisibleToOwner);
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
