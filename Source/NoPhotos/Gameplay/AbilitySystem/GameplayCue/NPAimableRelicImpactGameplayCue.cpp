#include "Gameplay/AbilitySystem/GameplayCue/NPAimableRelicImpactGameplayCue.h"

#include "Components/SceneComponent.h"
#include "Core/GameplayTag/NPGameplayTags.h"
#include "NiagaraComponent.h"

ANPAimableRelicImpactGameplayCue::ANPAimableRelicImpactGameplayCue()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	bAutoAttachToOwner = false;
	GameplayCueTag = NPGameplayTags::GameplayCue_Relic_Aimable_Impact;
	GameplayCueName = GameplayCueTag.GetTagName();

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	ImpactEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("ImpactEffect"));
	ImpactEffect->SetupAttachment(SceneRoot);
	ImpactEffect->SetAutoActivate(false);
}

bool ANPAimableRelicImpactGameplayCue::OnExecute_Implementation(
	AActor* Target,
	const FGameplayCueParameters& Parameters)
{
	Super::OnExecute_Implementation(Target, Parameters);
	const FHitResult* HitResult = Parameters.EffectContext.GetHitResult();
	const FVector Location = HitResult
		? FVector(HitResult->ImpactPoint)
		: FVector(Parameters.Location);
	const FVector Normal = HitResult
		? FVector(HitResult->ImpactNormal)
		: FVector(Parameters.Normal);
	SetActorLocationAndRotation(Location, Normal.Rotation());

	if (GetNetMode() == NM_DedicatedServer || !ImpactEffect->GetAsset())
	{
		GameplayCueFinishedCallback();
		return true;
	}

	ImpactEffect->OnSystemFinished.AddUniqueDynamic(
		this,
		&ANPAimableRelicImpactGameplayCue::HandleSystemFinished);
	bIsPlaying = true;
	ImpactEffect->ReinitializeSystem();
	return true;
}

bool ANPAimableRelicImpactGameplayCue::GameplayCuePendingRemove()
{
	return bIsPlaying || Super::GameplayCuePendingRemove();
}

bool ANPAimableRelicImpactGameplayCue::Recycle()
{
	bIsPlaying = false;
	ImpactEffect->OnSystemFinished.RemoveDynamic(
		this,
		&ANPAimableRelicImpactGameplayCue::HandleSystemFinished);
	ImpactEffect->DeactivateImmediate();
	return Super::Recycle();
}

void ANPAimableRelicImpactGameplayCue::HandleSystemFinished(
	UNiagaraComponent*)
{
	GameplayCueFinishedCallback();
}
