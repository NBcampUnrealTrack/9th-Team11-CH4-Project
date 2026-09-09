#include "Gameplay/AbilitySystem/GameplayCue/NPAimableRelicFireGameplayCue.h"

#include "Components/SceneComponent.h"
#include "Core/GameplayTag/NPGameplayTags.h"
#include "NiagaraComponent.h"

ANPAimableRelicFireGameplayCue::ANPAimableRelicFireGameplayCue()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	bAutoAttachToOwner = false;
	GameplayCueTag = NPGameplayTags::GameplayCue_Relic_Aimable_Fire;
	GameplayCueName = GameplayCueTag.GetTagName();

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	MuzzleEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("MuzzleEffect"));
	MuzzleEffect->SetupAttachment(SceneRoot);
	MuzzleEffect->SetAutoActivate(false);
}

bool ANPAimableRelicFireGameplayCue::OnExecute_Implementation(
	AActor* Target,
	const FGameplayCueParameters& Parameters)
{
	Super::OnExecute_Implementation(Target, Parameters);
	SetActorLocationAndRotation(
		Parameters.Location,
		FVector(Parameters.Normal).Rotation());

	if (GetNetMode() == NM_DedicatedServer || !MuzzleEffect->GetAsset())
	{
		GameplayCueFinishedCallback();
		return true;
	}

	MuzzleEffect->OnSystemFinished.AddUniqueDynamic(
		this,
		&ANPAimableRelicFireGameplayCue::HandleSystemFinished);
	bIsPlaying = true;
	MuzzleEffect->ReinitializeSystem();
	return true;
}

bool ANPAimableRelicFireGameplayCue::GameplayCuePendingRemove()
{
	return bIsPlaying || Super::GameplayCuePendingRemove();
}

bool ANPAimableRelicFireGameplayCue::Recycle()
{
	bIsPlaying = false;
	MuzzleEffect->OnSystemFinished.RemoveDynamic(
		this,
		&ANPAimableRelicFireGameplayCue::HandleSystemFinished);
	MuzzleEffect->DeactivateImmediate();
	return Super::Recycle();
}

void ANPAimableRelicFireGameplayCue::HandleSystemFinished(
	UNiagaraComponent*)
{
	GameplayCueFinishedCallback();
}
